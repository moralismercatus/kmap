/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "task.hpp"

#include "cmd/log.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "error/result.hpp"
#include "kmap.hpp"
#include "path/node_view.hpp"
#include "tag/tag.hpp"
#include "test/util.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap {

auto fetch_task_root( Kmap& kmap )
    -> Result< Uuid >
{
    return KTRY( view::make( kmap.root_node_id() )
               | view::child( "task" )
               | view::fetch_or_create_node( kmap ) );
}

TaskStore::TaskStore( Kmap& kmap )
    : kmap_{ kmap }
    , eclerk_{ kmap }
{
}

auto TaskStore::cascade_tags( Uuid const& task )
    -> Result< void >
{
    auto rv = error::make_result< void >();

    auto const rtags = view::make( task )
                     | view::tag // further refine: exclude tags from tag.status (open, close, etc.). We don't want these tags to cascade. They're individual statuses for each node.
                     | view::resolve
                     | view::to_node_set( kmap_ );

    // TODO:
    // KTRY( view::make( task )
    //     | view::desc( view::PredFn{ [ this ]( auto const& n ){ return is_task( n ); } } ) // TODO: Replace with `view::desc | view::task`
    //     | view::tag( rtags )
    //     | view::fetch_or_create_node( kmap_ ) );
    //     Discussion:
    //     Basically, the problem is, tag( <set> ) doesn't understand fetch_or_create, it only understands fetch_all( <set> ) and create_all( <set> ).
    //     Discussed in detail elsewhere, along with possible "currying" solution.
    //     For now, this is a workaround:
    auto const subtasks = view::make( task )
                        | view::desc( view::PredFn{ [ this ]( auto const& n ){ return is_task( n ); } } ) // TODO: Replace with `view::desc | view::task`
                        | view::to_node_set( kmap_ );

    for( auto const& subtask : subtasks )
    {
        for( auto const& rtag : rtags )
        {
            if( auto const exists = view::make( subtask )
                                  | view::tag( rtag )
                                  | view::exists( kmap_ )
              ; !exists )
            {
                KTRY( tag_node( kmap_, subtask, rtag ) );
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

    auto const task_root = KTRY( fetch_task_root( kmap_ ) );
    auto const task = KTRY( kmap_.create_child( task_root, format_heading( title ), title ) );
    
    KTRY( view::make( task )
        | view::child( view::all_of( "problem", "result" ) )
        | view::create_node( kmap_ ) );

    KTRY( fetch_or_create_tag( kmap_, "status.open" ) );
    
    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.task" }, to_string( task ) ) );

    KTRY( open_task( task ) );

    rv = task;

    return rv;
}

SCENARIO( "create_task", "[task][create]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    kmap.init_event_store();
    kmap.init_task_store();
    auto& tstore = kmap.task_store();

    GIVEN( "no tasks" )
    {
        WHEN( "create task" )
        {
            auto const t1 = REQUIRE_TRY( tstore.create_task( "1" ) );

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

    auto const subtask = KTRY( create_task( title ) );
    auto const st_alias = KTRY( view::make( supertask )
                              | view::child( "subtask" )
                              | view::alias( subtask )
                              | view::create_node( kmap_ )
                              | view::to_single );

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.subtask" } ) );

    rv = st_alias;

    return rv;
}

SCENARIO( "TaskStore::create_subtask", "[task][create]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    kmap.init_event_store();
    kmap.init_task_store();
    auto& tstore = kmap.task_store();

    GIVEN( "no tasks" )
    {
        WHEN( "create subtask with task root as supertask" )
        {
            auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );

            REQUIRE( kmap::test::fail( tstore.create_subtask( troot, "1" ) ) );
        }
    }
    GIVEN( "one task" )
    {
        auto const super = REQUIRE_TRY( tstore.create_task( "super" ) );

        WHEN( "create subtask with 'super' as supertask" )
        {
            auto const sub = REQUIRE_TRY( tstore.create_subtask( super, "sub" ) );

            THEN( "'sub' is task" )
            {
                REQUIRE( tstore.is_task( sub ) );
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
auto TaskStore::is_categorized( Uuid const& task )
    -> bool
{
    auto const nonopen_tags = view::make( task )
                            | view::tag( view::none_of( "status.open" ) )
                            | view::to_node_set( kmap_ );

    return !nonopen_tags.empty();
}

auto TaskStore::is_task( Uuid const& node )
    -> bool
{
    auto rv = false;

    if( auto const troot = fetch_task_root( kmap_ )
      ; troot )
    {
        rv = kmap_.is_child( troot.value(), kmap_.resolve( node ) );
    }

    return rv;
}

auto TaskStore::close_task( Uuid const& task )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_task( task ), error_code::common::uncategorized );
    KMAP_ENSURE( !( view::make( task ) | view::tag( "status.closed" ) | view::exists( kmap_ ) ), error_code::common::uncategorized );
    KMAP_ENSURE( is_categorized( task ), error_code::common::uncategorized );

    // disallow closure of empty body results.
    auto const result_documented = [ & ]
    {
        auto const resultn = KTRYE( view::make( task )
                                  | view::child( "result" )
                                  | view::fetch_node( kmap_ ) );

        if( auto const body = kmap_.fetch_body( resultn )
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

            if( v | view::exists( kmap_ ) )
            {
                KTRY( v | view::erase_node( kmap_ ) );
            }
        }
        // Append close tag...
        {
            KTRY( fetch_or_create_tag( kmap_, "status.closed" ) );

            KTRY( view::make( task )
                | view::tag( "status.closed" )
                | view::create_node( kmap_ ) );
        }

        KTRY( eclerk_.fire_event( { "subject.task_store", "verb.closed", "object.task" }, to_string( task ) ) );

        rv = outcome::success();
    }

    return rv;
}

auto TaskStore::open_task( Uuid const& task )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( !( view::make( task ) | view::tag( "status.open" ) | view::exists( kmap_ ) ), error_code::common::uncategorized );

    auto const vtag = view::make( task )
                    | view::tag( "status.open" );
    // Remove closed tag if it exists.
    {
        if( vtag | view::exists( kmap_ ) )
        {
            KTRY( vtag | view::erase_node( kmap_ ) );
        }
    }
    // Append open tag.
    {
        KTRY( vtag | view::create_node( kmap_ ) );
    }

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.opened", "object.task" }, to_string( task ) ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap
