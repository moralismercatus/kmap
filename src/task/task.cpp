/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "task.hpp"

#include "cmd/log.hpp"
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

auto TaskStore::create_task( std::string const& title )
    -> Result< Uuid >
{
    auto rv = error::make_result< Uuid >();
    auto const task_root = KTRY( fetch_task_root( kmap_ ) );
    auto const task = KTRY( kmap_.create_child( task_root, format_heading( title ), title ) );
    
    KTRY( view::make( task )
        | view::child( view::all_of( "problem", "result" ) )
        | view::create_node( kmap_ ) );

    // create_tag( kmap, task, "task.open.active" );
    auto const open_tag = KTRY( fetch_or_create_tag( kmap_, "status.open.active" ) );
    
    KTRY( tag_node( kmap_, task, open_tag ) );

    // create tags: open.active
    auto const dl = KTRY( cmd::log::fetch_or_create_daily_log( kmap_ ) );

    KTRY( view::make( dl )
        | view::child( "task" )
        | view::alias( task )
        | view::fetch_or_create_node( kmap_ ) );

    // TODO: Probably should be a create.log event for which task automatically adds open tasks?

    // KTRY( kmap_.event_store().fire_event( { "subject.task", "verb.created" } ) );

    rv = task;

    return rv;
}

SCENARIO( "create_task", "[task]" )
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

auto close_task( Kmap& kmap
              , std::string const& heading )
    -> Result< void >
{
    // disallow closure of task until it has more tags than just open
    // disallow closure of empty body results.
    return outcome::success();
}

auto open_task( Kmap& kmap
              , std::string const& heading )
    -> Result< void >
{
    return outcome::success();
}

} // namespace kmap
