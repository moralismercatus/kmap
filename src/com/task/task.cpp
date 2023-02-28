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
#include "path/act/update_body.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"
#include "utility.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/stable_sort.hpp>

namespace kmap::com {

auto fetch_task_root( Kmap const& kmap )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    return KTRY( view::abs_root
               | view::child( "task" )
               | view::fetch_node( kmap ) );
}

auto fetch_task_root( Kmap& kmap )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    return KTRY( view::abs_root
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
    register_standard_commands();
}

auto TaskStore::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto TaskStore::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}

auto TaskStore::cascade_tags( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "task", task );

    auto rv = result::make_result< void >();
    auto& km = kmap_inst();
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const rtags = view::make( task )
                     | view::tag( view::none_of( "task.status.open.active", "task.status.open.inactive", "task.status.closed" ) )
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "title", title );

    auto rv = result::make_result< Uuid >();

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

    KTRY( tag_store->fetch_or_create_tag( "task.status.open.active" ) );
    
    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.task" }, to_string( task ) ) );

    KTRY( activate_task( task ) );

    KTRY( nw->set_ordering_position( task, 0 ) );

    rv = task;

    return rv;
}

SCENARIO( "create_task", "[task][create][tag][order]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& km = Singleton::instance();
    auto const tstore = REQUIRE_TRY( km.fetch_component< com::TaskStore >() );
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );

    GIVEN( "no tasks" )
    {
        GIVEN( "create.task 1" )
        {
            auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

            THEN( "structure correct" )
            {
                REQUIRE( 2 == ( view::make( t1 ) 
                              | view::child( view::all_of( "problem", "result" ) )
                              | view::count( km ) ) );
            }
            THEN( "1 is open and active" )
            {
                REQUIRE(( view::make( t1 )
                        | view::tag( "task.status.open.active" )
                        | view::exists( km ) ));
            }
            GIVEN( "create.task 2" )
            {
                auto const t2 = REQUIRE_TRY( tstore->create_task( "2" ) );

                THEN( "task is top of task list" )
                {
                    auto const t2_pos = REQUIRE_TRY( nw->fetch_ordering_position( t2 ) );

                    REQUIRE( t2_pos == 0 );
                }
            }
        }
    }
}

auto TaskStore::create_subtask( Uuid const& supertask
                              , std::string const& title )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "supertask", supertask );
        KM_RESULT_PUSH_STR( "title", title );

    auto rv = result::make_result< Uuid >();

    KMAP_ENSURE( is_task( supertask ), error_code::common::uncategorized );

    auto& km = kmap_inst();
    auto const subtask_node = KTRY( create_task( title ) );

    KTRY( view::make( supertask )
        | view::child( "subtask" )
        | view::alias( view::Alias::Src{ subtask_node } )
        | view::create_node( km )
        | view::to_single );

    auto const st_alias = KTRY( anchor::node( supertask )
                              | view2::child( "subtask" )
                              | view2::alias( view2::resolve( subtask_node ) )
                              | act2::fetch_node( km ) );

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.subtask" } ) );

    rv = st_alias;

    return rv;
}

// task = task_root | child( all_of( child( "problem" ), child( "result" ) ) )
// subtask = child( "subtask" ) | alias | current( task_root | task )
// What about stacking predicates... sure? Why not?:
// subtask = child( "subtask" )( parent( task ) ) | alias( task_root | task ) // OK: alias ready to accept another predicate i.e., name of subtask, etc.


SCENARIO( "TaskStore::create_subtask", "[task][create]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& km = Singleton::instance();
    auto tstore = REQUIRE_TRY( km.fetch_component< TaskStore >() );

    GIVEN( "no tasks" )
    {
        WHEN( "create subtask with task root as supertask" )
        {
            auto const troot = REQUIRE_TRY( fetch_task_root( km ) );

            REQUIRE( kmap::test::fail( tstore->create_subtask( troot, "1" ) ) );
        }
    }
    GIVEN( "create.task super" )
    {
        auto const super = REQUIRE_TRY( tstore->create_task( "super" ) );

        GIVEN( "create.subtask sub" )
        {
            auto const super_subtask_sub = REQUIRE_TRY( tstore->create_subtask( super, "sub" ) );

            THEN( "'sub' is task" )
            {
                REQUIRE( tstore->is_task( super_subtask_sub ) );
            }
            THEN( "result 'sub' is subtask to 'super'" )
            {
                auto const subalias = REQUIRE_TRY(( anchor::node( super )
                                                  | view2::child( "subtask" )
                                                  | view2::alias
                                                  | act2::fetch_node( km ) ));

                REQUIRE( subalias == super_subtask_sub );
            }

            GIVEN( "@super.subtask.sub :create.subtask subsub" )
            {
                auto const sub_subtask_subsub = REQUIRE_TRY( tstore->create_subtask( super_subtask_sub, "subsub" ) );

                THEN( "result 'subsub' is sub.subtask.subsub alias" )
                {
                    KTRYE( print_tree( km, super ) );
                    REQUIRE(( anchor::node( super )
                            | view2::child( "subtask" )
                            | view2::alias( "sub" )
                            | view2::child( "subtask" )
                            | view2::alias( sub_subtask_subsub )
                            | act2::exists( km ) ));
                }
                THEN( "'subsub' is task" )
                {
                    REQUIRE( tstore->is_task( sub_subtask_subsub ) );
                }
                THEN( "'subsub' is a subtask of resolve( 'sub' )" )
                {
                    // auto const rsub = REQUIRE_TRY(( anchor::node( sub_subtask_sub ) 
                    //                               | view2::resolve
                    //                               | act2::fetch_node( km ) ));
                    // auto const sub_subtask_sub = REQUIRE_TRY( anchor::node( rsub )
                    //                                         |  );
                }
                THEN( "'subsub' is subtask to 'sub'" )
                {
                    // auto subsubalias = REQUIRE_TRY( ( view::make( subalias )
                    //                                 | view::child( "subtask" )
                    //                                 | view::child
                    //                                 | view::fetch_node( km ) ) );

                    // REQUIRE( subsubalias == subsub );
                }
            }
        }
    }
}

// Return true if a tag other than `status.open` is found.
// TODO: Rather, task has tag other than tag.status desc found.
auto TaskStore::is_categorized( Uuid const& task ) const
    -> bool
{
    auto& kmap = kmap_inst();
    auto const nonopen_tags = view::make( task )
                            | view::tag( view::none_of( "task.status.open.active", "task.status.open.inactive" ) )
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
           | view::child( nw->resolve( node ) )
           | view::exists( km );
    }

    return rv;
}

auto TaskStore::close_task( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "task", task );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const tag_store = KTRY( fetch_component< TagStore >() );

    KMAP_ENSURE( is_task( task ), error_code::common::uncategorized );
    KMAP_ENSURE( !( view::make( task ) | view::tag( "task.status.closed" ) | view::exists( km ) ), error_code::common::uncategorized );
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
            if( auto const v = view::make( task )
                         | view::tag( view::any_of( "task.status.open.active", "task.status.open.inactive" ) )
              ; v | view::exists( km ) )
            {
                KTRY( v | view::erase_node( km ) );
            }
        }
        // Append close tag...
        {
            KTRY( tag_store->fetch_or_create_tag( "task.status.closed" ) );

            KTRY( view::make( task )
                | view::tag( "task.status.closed" )
                | view::create_node( km ) );
        }

        // Sort 
        {
            auto const task_root = KTRYE( fetch_task_root( km ) );
            auto tasks = view::make( task_root )
                             | view::child
                             | view::to_node_set( km )
                             | ranges::to< std::vector >();
            auto const pred = [ & ]( auto const& t1, auto const& t2 )
            {
                enum class Status : int { active, inactive, closed, nontask }; // Ordered.
                auto const map_status = [ & ]( auto const& n )
                {
                         if( view::make( n ) | view::tag( "task.status.open.active" ) | view::exists( km ) )  { return Status::active; }
                    else if( view::make( n ) | view::tag( "task.status.open.inactive" ) | view::exists( km ) ){ return Status::inactive; }
                    else if( view::make( n ) | view::tag( "task.status.closed" ) | view::exists( km ) )       { return Status::closed; }
                    else                                                                                      { return Status::nontask; }
                };

                return static_cast< int >( map_status( t1 ) ) < static_cast< int >( map_status( t2 ) ); 
            };

            ranges::stable_sort( tasks, pred ); // stable_sort maintains pre-sort order for equivalent items.

            KTRY( nw->reorder_children( task_root, tasks ) );
        }

        KTRY( eclerk_.fire_event( { "subject.task_store", "verb.closed", "object.task" }, to_string( task ) ) );
        // TODO: Provide outlet that moves closed node to top of all open task and subtask lists.

        rv = outcome::success();
    }

    return rv;
}

SCENARIO( "close_task", "[task][close]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& km = Singleton::instance();
    auto const tstore = REQUIRE_TRY( km.fetch_component< com::TaskStore >() );

    GIVEN( "no tasks" )
    {
        GIVEN( "close.task 1" )
        {
            auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

            REQUIRE(( view::make( t1 ) | view::tag( "task.status.open.active" ) | view::exists( km ) ));

            THEN( "close.task 1 fails" )
            {
                REQUIRE( test::fail( tstore->close_task( t1 ) ) );
            }
            GIVEN( "given category tag" )
            {
                auto const tag_store = REQUIRE_TRY( km.fetch_component< com::TagStore >() );

                REQUIRE_TRY( tag_store->create_tag( "test" ) );

                REQUIRE(( view::make( t1 ) | view::tag( "test" ) | view::create_node( km ) ));

                GIVEN( "result body nonempty" )
                {
                    REQUIRE_TRY(( view::make( t1 )
                                | view::child( "result" )
                                | act::update_body( km, "test" ) ));

                    WHEN( "close.task 1" )
                    {
                        REQUIRE_TRY( tstore->close_task( t1 ) );

                        THEN( "tag active exchanged for closed" )
                        {
                            REQUIRE( !( view::make( t1 ) | view::tag( "task.status.open.active" ) | view::exists( km ) ) );
                            REQUIRE(  ( view::make( t1 ) | view::tag( "task.status.closed" ) | view::exists( km ) ) );
                        }
                    }
                }
            }
        }
    }
}

auto TaskStore::activate_task( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "task", task );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& kmap = kmap_inst();
    auto const vactive_tag = view::tag( "task.status.open.active" );

    KMAP_ENSURE( !( view::make( task ) | vactive_tag | view::exists( kmap ) ), error_code::common::uncategorized ); // TODO: Needs to convey: "task already active"

    // Remove closed tag if it exists.
    if( auto const vt_closed = view::make( task )
                             | view::tag( "task.status.closed" )
      ; vt_closed | view::exists( kmap ) )
    {
        KTRY( vt_closed | view::erase_node( kmap ) );
    }
    if( auto const vt_inactive = view::make( task )
                               | view::tag( "task.status.open.inactive" )
      ; vt_inactive | view::exists( kmap ) )
    {
        KTRY( vt_inactive | view::erase_node( kmap ) );
    }
    // Append open tag.
    {
        auto const vt_active = view::make( task )
                           | vactive_tag;
        KTRY( vt_active | view::create_node( kmap ) );
    }

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.open.activated", "object.task" }, to_string( task ) ) );

    rv = outcome::success();

    return rv;
}

// TODO: unit test...

auto TaskStore::register_standard_commands()
    -> void
{
    using Guard = com::Command::Guard;
    using Argument = com::Command::Argument;

    // add.subtask
    {
        // guard: in_task()
        // action: :create.child subtask; :create.alias tag.$ctx.args[0]
    }
    // close.task
    {
        auto const guard_code = // TODO: Guard should include require: categorized && open
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
        const selected = kmap.selected_node();
        const res = kmap.task_store().close_task( selected );

        if( res.has_value() )
        {
            kmap.select_node( selected );

            return kmap.success( 'success' );
        }
        else
        {
            return kmap.failure( res.error_message() );
        }
        )%%%";
        auto const description = "closes task meeting requirements";
        auto const arguments = std::vector< Argument >{};

        cclerk_.register_command( com::Command{ .path = "close.task"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", guard_code }
                                              , .action = action_code } );
    }
    // create.task
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'unconditional' );
        )%%%";
        auto const action_code =
        R"%%%(
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
        )%%%";
        auto const description = "creates task";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        cclerk_.register_command( com::Command{ .path = "create.task"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "unconditional", guard_code }
                                              , .action = action_code } );
    }
    // create.subtask
    {
        auto const is_task_guard_code =
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
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
        )%%%";
        auto const description = "creates subtask";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        cclerk_.register_command( com::Command{ .path = "create.subtask"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", is_task_guard_code }
                                              , .action = action_code } );
    }
    // cascade.tags
    {
        auto const is_task_guard_code =
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
            const cascaded = kmap.task_store().cascade_tags( kmap.selected_node() );

            if( cascaded.has_value() )
            {
                kmap.select_node( kmap.selected_node() );

                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( cascaded.error_message() );
            }
        )%%%";
        auto const description = "Applies supertask tags to subtasks";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "is_task", is_task_guard_code };

        cclerk_.register_command( com::Command{ .path = "cascade.tags"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", is_task_guard_code }
                                              , .action = action_code } );
    }
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
    auto activate_task( kmap::Uuid const& task )
                   
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->activate_task( task );
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
        .function( "activate_task", &::TaskStore::activate_task )
        .function( "is_task", &::TaskStore::is_task )
        ;
}