/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "task.hpp"

#include "com/cmd/cclerk.hpp"
#include "com/network/network.hpp"
#include "com/tag/tag.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "error/result.hpp"
#include "kmap.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::com {

auto fetch_task_root( Kmap const& kmap )
    -> Result< Uuid >
{
    return KTRY( view::make( kmap.root_node_id() )
               | view::child( "task" )
               | view::fetch_node( kmap ) );
}

auto fetch_task_root( Kmap& kmap )
    -> Result< Uuid >
{
    return KTRY( view::make( kmap.root_node_id() )
               | view::child( "task" )
               | view::fetch_or_create_node( kmap ) );
}

TaskStore::TaskStore( Kmap& kmap
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ kmap, requisites, description }
    , eclerk_{ kmap }
    , cclerk_{ kmap }
{
}

auto TaskStore::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    fmt::print( "task_store :: initialize\n" );

    KTRY( install_standard_commands() );

    rv = outcome::success();

    return rv;
}

auto TaskStore::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto TaskStore::cascade_tags( Uuid const& task )
    -> Result< void >
{
    auto rv = error::make_result< void >();
    auto& km = kmap_inst();
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const rtags = view::make( task )
                     | view::tag // further refine: exclude tags from tag.status (open, close, etc.). We don't want these tags to cascade. They're individual statuses for each node.
                     | view::resolve
                     | view::to_node_set( km );

    // TODO:
    // KTRY( view::make( task )
    //     | view::desc( view::PredFn{ [ this ]( auto const& n ){ return is_task( n ); } } ) // TODO: Replace with `view::desc | view::task`
    //     | view::tag( rtags )
    //     | view::fetch_or_create_node( kmap ) );
    //     Discussion:
    //     Basically, the problem is, tag( <set> ) doesn't understand fetch_or_create, it only understands fetch_all( <set> ) and create_all( <set> ).
    //     Discussed in detail elsewhere, along with possible "currying" solution.
    //     For now, this is a workaround:
    auto const subtasks = view::make( task )
                        | view::desc( view::PredFn{ [ this ]( auto const& n ){ return is_task( n ); } } ) // TODO: Replace with `view::desc | view::task`
                        | view::to_node_set( km );

    for( auto const& subtask : subtasks )
    {
        for( auto const& rtag : rtags )
        {
            if( auto const exists = view::make( subtask )
                                  | view::tag( rtag )
                                  | view::exists( km )
              ; !exists )
            {
                KTRY( tag_store->tag_node( subtask, rtag ) );
            }
        }
    }

    rv = outcome::success();

    return rv;
}

auto TaskStore::create_task( std::string const& title )
    -> Result< Uuid >
{
    auto rv = error::make_result< Uuid >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_task( rv.value() ) );
            }
        })
    ;

    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const task_root = KTRY( fetch_task_root( km ) );
    auto const task = KTRY( nw->create_child( task_root, format_heading( title ), title ) );
    
    KTRY( view::make( task )
        | view::child( view::all_of( "problem", "result" ) )
        | view::create_node( km ) );

    KTRY( tag_store->fetch_or_create_tag( "status.open" ) );
    
    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.task" }, to_string( task ) ) );

    KTRY( open_task( task ) );

    rv = task;

    return rv;
}

SCENARIO( "create_task", "[task][create]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& kmap = Singleton::instance();
    auto const tstore = REQUIRE_TRY( kmap.fetch_component< TaskStore >() );

    GIVEN( "no tasks" )
    {
        WHEN( "create task" )
        {
            auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

            THEN( "structure matches" )
            {
                REQUIRE( 2 == ( view::make( t1 ) 
                              | view::child( view::all_of( "problem", "result" ) )
                              | view::count( kmap ) ) );
            }
            THEN( "is open and active" )
            {
            }
            THEN( "daily log entry made" )
            {
                // TODO: This test belongs in the bridge.
            }
        }
    }
}

auto TaskStore::create_subtask( Uuid const& supertask
                              , std::string const& title )
    -> Result< Uuid >
{
    auto rv = error::make_result< Uuid >();

    KMAP_ENSURE( is_task( supertask ), error_code::common::uncategorized );

    auto& kmap = kmap_inst();
    auto const subtask = KTRY( create_task( title ) );
    auto const st_alias = KTRY( view::make( supertask )
                              | view::child( "subtask" )
                              | view::alias( subtask )
                              | view::create_node( kmap )
                              | view::to_single );

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.subtask" } ) );

    rv = st_alias;

    return rv;
}

SCENARIO( "TaskStore::create_subtask", "[task][create]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& kmap = Singleton::instance();
    auto tstore = REQUIRE_TRY( kmap.fetch_component< TaskStore >() );

    GIVEN( "no tasks" )
    {
        WHEN( "create subtask with task root as supertask" )
        {
            auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );

            REQUIRE( kmap::test::fail( tstore->create_subtask( troot, "1" ) ) );
        }
    }
    GIVEN( "one task" )
    {
        auto const super = REQUIRE_TRY( tstore->create_task( "super" ) );

        WHEN( "create subtask with 'super' as supertask" )
        {
            auto const sub = REQUIRE_TRY( tstore->create_subtask( super, "sub" ) );

            THEN( "'sub' is task" )
            {
                REQUIRE( tstore->is_task( sub ) );
            }
            THEN( "'sub' is subtask to 'super'" )
            {
                auto subalias = REQUIRE_TRY( ( view::make( super )
                                             | view::child( "subtask" )
                                             | view::child
                                             | view::fetch_node( kmap ) ) );

                REQUIRE( subalias == sub );
            }
        }
    }
}

// Return true if a tag other than `status.open` is found.
auto TaskStore::is_categorized( Uuid const& task ) const
    -> bool
{
    auto& kmap = kmap_inst();
    auto const nonopen_tags = view::make( task )
                            | view::tag( view::none_of( "status.open" ) )
                            | view::to_node_set( kmap );

    return !nonopen_tags.empty();
}

auto TaskStore::is_task( Uuid const& node ) const
    -> bool
{
    auto rv = false;
    auto& km = kmap_inst();

    if( auto const troot = fetch_task_root( km )
      ; troot )
    {
        auto const nw = KTRYE( km.fetch_component< com::Network >() );

        rv = view::make( troot.value() )
           | view::child( nw->alias_store().resolve( node ) )
           | view::exists( km );
    }

    return rv;
}

auto TaskStore::close_task( Uuid const& task )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const tag_store = KTRY( fetch_component< TagStore >() );

    KMAP_ENSURE( is_task( task ), error_code::common::uncategorized );
    KMAP_ENSURE( !( view::make( task ) | view::tag( "status.closed" ) | view::exists( km ) ), error_code::common::uncategorized );
    KMAP_ENSURE( is_categorized( task ), error_code::common::uncategorized );

    // disallow closure of empty body results.
    auto const result_documented = [ & ]
    {
        auto const resultn = KTRYE( view::make( task )
                                  | view::child( "result" )
                                  | view::fetch_node( km ) );

        if( auto const body = nw->fetch_body( resultn )
          ; body )
        {
            return !body.value().empty();
        }
        else
        {
            return false;
        }
    }();
    
    if( result_documented && is_categorized( task ) )
    {
        // Remove open tag if it exists...
        {
            auto const v = view::make( task )
                         | view::tag( "status.open" );

            if( v | view::exists( km ) )
            {
                KTRY( v | view::erase_node( km ) );
            }
        }
        // Append close tag...
        {
            KTRY( tag_store->fetch_or_create_tag( "status.closed" ) );

            KTRY( view::make( task )
                | view::tag( "status.closed" )
                | view::create_node( km ) );
        }

        KTRY( eclerk_.fire_event( { "subject.task_store", "verb.closed", "object.task" }, to_string( task ) ) );

        rv = outcome::success();
    }

    return rv;
}

// TODO: unit test...

auto TaskStore::open_task( Uuid const& task )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& kmap = kmap_inst();

    KMAP_ENSURE( !( view::make( task ) | view::tag( "status.open" ) | view::exists( kmap ) ), error_code::common::uncategorized );

    // Remove closed tag if it exists.
    {
        auto const vt_closed = view::make( task )
                             | view::tag( "status.closed" );

        if( vt_closed | view::exists( kmap ) )
        {
            KTRY( vt_closed | view::erase_node( kmap ) );
        }
    }
    // Append open tag.
    {
        auto const vt_open = view::make( task )
                            | view::tag( "status.open" );
        KTRY( vt_open | view::create_node( kmap ) );
    }

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.opened", "object.task" }, to_string( task ) ) );

    rv = outcome::success();

    return rv;
}

// TODO: unit test...

auto TaskStore::install_standard_commands()
    -> Result< void >
{
    using Guard = com::Command::Guard;
    using Argument = com::Command::Argument;

    auto rv = KMAP_MAKE_RESULT( void );

    // create.task
    {
        auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
        auto const action_code =
R"%%%(```javascript
const taskn = kmap.task_store().create_task( args.get( 0 ) );

if( taskn.has_value() )
{
    kmap.select_node( taskn.value() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( taskn.error_message() );
}
```)%%%";
        auto const description = "creates task";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        KTRY( cclerk_.install_command( com::Command{ .path = "create.task"
                                                   , .description = description
                                                   , .arguments = arguments 
                                                   , .guard = Guard{ "unconditional", guard_code }
                                                   , .action = action_code } ) );
    }
    // create.subtask
    {
        auto const is_task_guard_code =
R"%%%(```javascript
if( kmap.task_store().is_task( kmap.selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not task' );
}
```)%%%";
        auto const action_code =
R"%%%(```javascript
const taskn = kmap.task_store().create_subtask( kmap.selected_node(), args.get( 0 ) );

if( taskn.has_value() )
{
    kmap.select_node( taskn.value() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( taskn.error_message() );
}
```)%%%";
        auto const description = "creates subtask";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        KTRY( cclerk_.install_command( com::Command{ .path = "create.subtask"
                                                   , .description = description
                                                   , .arguments = arguments 
                                                   , .guard = Guard{ "is_task", is_task_guard_code }
                                                   , .action = action_code } ) );
    }
    // cascade.tags
    {
        auto const is_task_guard_code =
R"%%%(```javascript
if( kmap.task_store().is_task( kmap.selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not task' );
}
```)%%%";
        auto const action_code =
R"%%%(```javascript
const closed = kmap.task_store().cascade_tags( kmap.selected_node() );

if( closed.has_value() )
{
    kmap.select_node( kmap.selected_node() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( closed.error_message() );
}
```)%%%";
        auto const description = "Applies supertask tags to subtasks";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "is_task", is_task_guard_code };

        KTRY( cclerk_.install_command( com::Command{ .path = "cascade.tags"
                                                   , .description = description
                                                   , .arguments = arguments 
                                                   , .guard = Guard{ "is_task", is_task_guard_code }
                                                   , .action = action_code } ) );
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap

namespace {
namespace task_store_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::TaskStore
,   std::set({ "command_store"s, "event_store"s, "tag_store"s })
,   "task related functionality"
);

} // namespace task_store_def 

struct TaskStore
{
    kmap::Kmap& km;

    TaskStore( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto cascade_tags( kmap::Uuid const& task )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->cascade_tags( task );
    }
    auto create_task( std::string const& title )
        -> kmap::binding::Result< kmap::Uuid >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->create_task( title );
    }
    auto create_subtask( kmap::Uuid const& supertask
                       , std::string const& title )
        -> kmap::binding::Result< kmap::Uuid >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->create_subtask( supertask, title );
    }
    auto close_task( kmap::Uuid const& task )
                   
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->close_task( task );
    }
    auto open_task( kmap::Uuid const& task )
                   
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->open_task( task );
    }
    auto is_task( kmap::Uuid const& task )
        -> bool
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->is_task( task );
    }
};

auto task_store()
    -> ::TaskStore
{
    return ::TaskStore{ kmap::Singleton::instance() };
}

} // namespace anonymous

using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_task_store )
{
    function( "task_store", &::task_store );

    class_< ::TaskStore >( "TaskStore" )
        .function( "cascade_tags", &::TaskStore::cascade_tags )
        .function( "create_task", &::TaskStore::create_task )
        .function( "create_subtask", &::TaskStore::create_subtask )
        .function( "close_task", &::TaskStore::close_task )
        .function( "open_task", &::TaskStore::open_task )
        .function( "is_task", &::TaskStore::is_task )
        ;
}