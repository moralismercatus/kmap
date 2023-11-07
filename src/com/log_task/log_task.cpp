/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/log_task/log_task.hpp>

#include <com/event/event.hpp>
#include <com/event/event_clerk.hpp>
#include <com/network/network.hpp>
#include <com/log/log.hpp>
#include <com/tag/tag.hpp>
#include <com/task/task.hpp>
#include <component.hpp>
#include <contract.hpp>
#include <kmap.hpp>
#include <path/act/order.hpp>
#include <path/node_view.hpp>
#include <test/util.hpp>
#include <util/result.hpp>

#include <catch2/catch_test_macros.hpp>

namespace kmap::com {

LogTask::LogTask( Kmap& kmap
                , std::set< std::string > const& requisites
                , std::string const& description )
    : Component{ kmap, requisites, description }
    , eclerk_{ kmap }
{
    KM_RESULT_PROLOG();

    KTRYE( register_standard_events() );
}

auto LogTask::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto LogTask::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}

auto LogTask::register_standard_events()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // Note: Under domain of "log", as no change is actually made to task, all changes made to log.
    eclerk_.register_outlet( Leaf{ .heading = "log.add_task_when_task_activated"
                                    , .requisites = { "subject.task_store", "verb.open.activated", "object.task" }
                                    , .description = "adds task to log task list when a task is opened"
                                    , .action = R"%%%(kmap.log_task().push_task_to_log();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "log.add_active_tasks_on_daily_log_creation"
                                    , .requisites = { "subject.log", "verb.created", "object.daily" }
                                    , .description = "adds active tasks to task list when new daily log is created"
                                    , .action = R"%%%(kmap.log_task().push_active_tasks_to_log();)%%%" } );
    
    rv = outcome::success();

    return rv;
}

auto LogTask::push_task_to_log()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    auto& kmap = kmap_inst();
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const payload = KTRY( estore->fetch_payload() );
    auto const task = KTRY( uuid_from_string( std::string{ payload.at( "task_id" ).as_string() } ) );
    auto const dl = KTRY( com::fetch_or_create_daily_log( kmap ) );

    KMAP_ENSURE( nw->exists( task ), error_code::network::invalid_node );

    KTRY( view::make( dl )
        | view::child( "task" )
        | view::alias( view::Alias::Src{ task } )
        | view::fetch_or_create_node( kmap ) );

    rv = outcome::success();

    return rv;
}

auto LogTask::push_active_tasks_to_log()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto& km = kmap_inst();
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const active_tag = KTRY( tag_store->fetch_tag( "task.status.open.active" ) ); 
    auto const has_active_tag = [ & ]( Uuid const& n )
    {
        return tag_store->has_tag( n, active_tag );
    };
    auto rv = KMAP_MAKE_RESULT( void );
    auto const task_root = KTRY( fetch_task_root( km ) );
    auto const active_tasks = view::make( task_root )
                            | view::child( view::PredFn{ has_active_tag } ) // TODO: Need view::task instead of view::child, as a non-task could be a child.
                            | view::to_node_set( km )
                            | act::order( km );
    auto const dl = KTRY( com::fetch_or_create_daily_log( km ) );

    for( auto const& active_task : active_tasks )
    {
        KTRY( view::make( dl )
            | view::child( "task" )
            | view::alias( view::Alias::Src{ active_task } ) // TODO: Why not view::alias( open_tasks [UuidVec]? )
            | view::fetch_or_create_node( km ) );
    }
    
    rv = outcome::success();

    return rv;
}

SCENARIO( "push active tasks to new daily log", "[cmd][log][task]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "log_task", "cli" );

    auto& kmap = Singleton::instance();
    auto const cli = REQUIRE_TRY( kmap.fetch_component< com::Cli >() );
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    
    GIVEN( "no tasks" )
    {
        GIVEN( "no daily log" )
        {
            WHEN( "create.daily.log" )
            {
                REQUIRE_RES( cli->parse_raw( ":create.daily.log" ) );

                THEN( "no log tasks" )
                {
                    auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                    
                    REQUIRE( !( view::make( dl )
                              | view::child( "task" )
                              | view::exists( kmap ) ) );
                }
                WHEN( "create.task" )
                {
                    REQUIRE_RES( cli->parse_raw( ":create.task Pass Test" ) );

                    WHEN( "activate.task" )
                    {
                        REQUIRE_RES( cli->parse_raw( ":activate.task" ) );

                        THEN( "task pushed to daily log" )
                        {
                            auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                            auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
                            auto const task = REQUIRE_TRY( view::make( troot )
                                                        | view::child( "pass_test" )
                                                        | view::fetch_node( kmap ) );
                            auto const dlt = REQUIRE_TRY( view::make( dl )
                                                        | view::child( "task" )
                                                        | view::alias
                                                        | view::fetch_node( kmap ) );

                            REQUIRE( task == nw->resolve( dlt ) );
                        }
                    }
                }
            }
            WHEN( "create.task" )
            {
                REQUIRE_RES( cli->parse_raw( ":create.task Pass Test" ) );

                WHEN( "activate.task" )
                {
                    REQUIRE_RES( cli->parse_raw( ":activate.task" ) );

                    THEN( "task pushed to daily log" )
                    {
                        auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                        auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
                        auto const task = REQUIRE_TRY( view::make( troot )
                                                    | view::child( "pass_test" )
                                                    | view::fetch_node( kmap ) );
                        auto const dlt = REQUIRE_TRY( view::make( dl )
                                                    | view::child( "task" )
                                                    | view::alias
                                                    | view::fetch_node( kmap ) );

                        REQUIRE( task == nw->resolve( dlt ) );
                    }
                }

                WHEN( "create.subtask" )
                {
                    REQUIRE_RES( cli->parse_raw( ":create.subtask Subtask" ) );

                    WHEN( "activate.task" )
                    {
                        REQUIRE_RES( cli->parse_raw( ":activate.task" ) );

                        THEN( "subtask pushed to daily log" )
                        {
                            auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                            auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
                            auto const subtask = REQUIRE_TRY( view::make( troot )
                                                            | view::child( "subtask" )
                                                            | view::fetch_node( kmap ) );
                            auto const dlt = REQUIRE_TRY( view::make( dl )
                                                        | view::child( "task" )
                                                        | view::alias( view::Alias::Src{ subtask } )
                                                        | view::fetch_node( kmap ) );

                            REQUIRE( subtask == nw->resolve( dlt ) );
                        }
                    }
                }
            }
        }
    }
    GIVEN( "one active task" )
    {
        REQUIRE_RES( cli->parse_raw( ":create.task Pass Test" ) );
        REQUIRE_RES( cli->parse_raw( ":activate.task" ) );

        {
            auto const ln = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
            REQUIRE_RES( nw->erase_node( ln ) );
        }

        GIVEN( "no daily log" )
        {
            WHEN( "create.daily.log" )
            {
                REQUIRE_RES( cli->parse_raw( ":create.daily.log" ) );

                THEN( "active task pushed to daily log" )
                {
                    auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                    auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
                    auto const task = REQUIRE_TRY( view::make( troot )
                                                 | view::child( "pass_test" )
                                                 | view::fetch_node( kmap ) );
                    auto const dlt = REQUIRE_TRY( view::make( dl )
                                                | view::child( "task" )
                                                | view::alias
                                                | view::fetch_node( kmap ) );

                    REQUIRE( task == nw->resolve( dlt ) );
                }
            }
        }
    }
    AND_GIVEN( "another open task")
    {
        // TODO: ensure order of task.<alias> matches when :create.daily.log
    }
    // AND_GIVEN( "one closed task")
    // {
    //     REQUIRE_RES( cli.parse_raw( ":create.task Close Test" ) );
    //     REQUIRE_RES( cli.parse_raw( ":close.task" ) );

    //     WHEN( "create.daily.log" )
    //     {
    //         REQUIRE_RES( cli.parse_raw( ":create.daily.log" ) );

    //         THEN( "only open task pushed to daily log" )
    //         {
    //             auto const dl = REQUIRE_TRY( cmd::log::fetch_daily_log( kmap ) );
    //             auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
    //             auto const task = REQUIRE_TRY( view::make( troot )
    //                                             | view::child( "pass_test" )
    //                                             | view::fetch_node( kmap ) );
    //             auto const dlt = REQUIRE_TRY( view::make( dl )
    //                                         | view::child( "task" )
    //                                         | view::alias
    //                                         | view::fetch_node( kmap ) );

    //             REQUIRE( task == kmap.resolve( dlt ) );
    //         }
    //     }
    // }
}

} // namespace kmap::com

namespace {
namespace log_task_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::LogTask
,   std::set({ "event_store"s, "log_store"s, "task_store"s })
,   "responsible for task events related to log"
);

} // namespace log_task_def 
} // namespace anonymous
