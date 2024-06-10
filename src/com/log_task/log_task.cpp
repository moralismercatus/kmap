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
    , cclerk_{ kmap }
{
    KM_RESULT_PROLOG();

    KTRYE( register_standard_events() );
    KTRYE( register_standard_commands() );
}

auto LogTask::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.install_registered() );
    KTRY( cclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto LogTask::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.check_registered() );
    KTRY( cclerk_.check_registered() );

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
                                 , .action = R"%%%(ktry( kmap.log_task().push_active_tasks_to_log() );)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "log.add_active_tasks_on_daily_log_creation"
                                 , .requisites = { "subject.log", "verb.created", "object.daily" }
                                 , .description = "adds active tasks to task list when new daily log is created"
                                 , .action = R"%%%(ktry( kmap.log_task().push_active_tasks_to_log() );)%%%" } );
    
    rv = outcome::success();

    return rv;
}

auto LogTask::register_standard_commands()
    -> Result< void >
{
    using Argument = com::Command::Argument;

    KM_RESULT_PROLOG();

    // guard.log.daily
    {
        // auto const guard_code =
        // R"%%%(
        //     /* TODO: Selected node must be a daily log*/
        // )%%%";

        // auto const description = "daily log node";

        // KTRY( cclerk_.register_guard( Guard{ .path = "log.daily"
        //                                     , .description = description
        //                                     , .action = guard_code } ) );
    }
    // TODO: This belongs in log.cpp
    // arg.log.daily
    {
        auto const guard_code =
        R"%%%(
            if( !kmap.is_valid_heading_path( args.get( 0 ) ) )
            {
                return kmap.failure( 'invalid heading path' );
            }
        )%%%";
        auto const completion_code =
        R"%%%(
            const root = kmap.fetch_node( "/log.daily" );

            if( root.has_error() )
            {
                return new kmap.VectorString();
            }
            else
            {
                return kmap.complete_heading_path_from( root.value(), root.value(), arg );
            }
        )%%%";

        auto const description = "accumulates and prints daily tasks";
        
        KTRY( cclerk_.register_argument( com::Argument{ .path = "log.daily"
                                                      , .description = description
                                                      , .guard = guard_code
                                                      , .completion = completion_code } ) );
    }
    // print.task.from 
    {
        auto const action_code =
        R"%%%(
        const lt = kmap.log_task();
        const nw = kmap.network();
        const from = ktry( nw.fetch_node( args.get( 0 ) ) );
        const to = nw.selected_node();

        ktry( lt.print_tasks( from, to ) );
        )%%%";
        auto const description = "accumulates and prints daily tasks";
        auto const arguments = std::vector< Argument >{ Argument{ .heading = "log_date"
                                                                , .description = "yyyy.mm.dd"
                                                                , .argument_alias = "log.daily" } };

        KTRY( cclerk_.register_command( com::Command{ .path = "print.task.from"
                                                    , .description = description
                                                    , .arguments = arguments 
                                                    , .guard = "unconditional" // TODO: kmap.task_store().is_task( kmap.selected_node() )
                                                    , .action = action_code } ) );
    }
    // log.task
    {
        // TODO: Also, this should work if in task tree, so selected should be taken from that.
        auto const action_code =
        R"%%%(
        const lt = kmap.log_task();
        const nw = kmap.network();
        const selected = nw.selected_node();

        ktry( lt.push_task_to_daily_log( selected ) );
        )%%%";
        auto const description = "accumulates and prints daily tasks";
        auto const arguments = std::vector< Argument >{};

        KTRY( cclerk_.register_command( com::Command{ .path = "log.task"
                                                    , .description = description
                                                    , .arguments = arguments 
                                                    , .guard = "unconditional" // TODO: kmap.task_store().is_task( kmap.selected_node() )
                                                    , .action = action_code } ) );
    }

    return outcome::success();
}

auto LogTask::push_task_to_daily_log( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    auto& kmap = kmap_inst();
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const dl = KTRY( com::fetch_or_create_daily_log( kmap ) );

    KMAP_ENSURE( nw->exists( task ), error_code::network::invalid_node );

    KTRY( anchor::node( dl )
        | view2::child( "task" )
        | view2::alias_src( task )
        | act2::fetch_or_create_node( kmap )
        | act2::single );

    rv = outcome::success();

    return rv;
}

auto LogTask::push_active_tasks_to_log()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto& km = kmap_inst();
    auto rv = KMAP_MAKE_RESULT( void );
    auto const active_tasks = view2::task::task_root
                            | view2::child( view2::attrib::tag( view2::resolve( view2::task::active_tag ) ) )
                            | act2::to_node_set( km );
    auto const dl = KTRY( com::fetch_or_create_daily_log( km ) );

    if( !active_tasks.empty() )
    {
        KTRY( anchor::node( dl )
            | view2::child( "task" )
            | view2::all_of( view2::alias_src, active_tasks )
            | act2::fetch_or_create_node( km ) );
    }
    
    rv = outcome::success();

    return rv;
}

SCENARIO( "activated tasks pushed to daily log", "[cmd][log][task]" )
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
                    
                    REQUIRE( !( anchor::node( dl )
                              | view2::child( "task" )
                              | act2::exists( kmap ) ) );
                }
                WHEN( "create.task" )
                {
                    REQUIRE_RES( cli->parse_raw( ":create.task t1" ) );

                    WHEN( "activate.task <t1>" )
                    {
                        REQUIRE_RES( cli->parse_raw( ":activate.task" ) );

                        THEN( "task pushed to daily log" )
                        {
                            auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                            auto const task = REQUIRE_TRY( view2::task::task_root
                                                         | view2::child( "t1" )
                                                         | act2::fetch_node( kmap ) );
                            auto const dlt = REQUIRE_TRY( anchor::node( dl )
                                                        | view2::child( "task" )
                                                        | view2::alias
                                                        | act2::fetch_node( kmap ) );

                            REQUIRE( task == nw->resolve( dlt ) );
                        }
                    }
                }
            }
            WHEN( "create.task <t1>" )
            {
                REQUIRE_RES( cli->parse_raw( ":create.task t1" ) );

                WHEN( "activate.task <t1>" )
                {
                    REQUIRE_RES( cli->parse_raw( ":activate.task" ) );

                    THEN( "task pushed to daily log" )
                    {
                        auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                        auto const t1 = REQUIRE_TRY( view2::task::task_root
                                                     | view2::child( "t1" )
                                                     | act2::fetch_node( kmap ) );
                        auto const dlt = REQUIRE_TRY( anchor::node( dl )
                                                    | view2::child( "task" )
                                                    | view2::alias
                                                    | act2::fetch_node( kmap ) );

                        REQUIRE( t1 == nw->resolve( dlt ) );
                    }
                }

                WHEN( "create.subtask t2" )
                {
                    REQUIRE_RES( cli->parse_raw( ":create.subtask t2" ) );

                    WHEN( "activate.task <t2>" )
                    {
                        REQUIRE_RES( cli->parse_raw( ":activate.task" ) );

                        THEN( "t2 pushed to daily log" )
                        {
                            auto const dl = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
                            auto const t2 = REQUIRE_TRY( view2::task::task_root
                                                            | view2::child( "t2" )
                                                            | act2::fetch_node( kmap ) );
                            auto const dlt = REQUIRE_TRY( anchor::node( dl )
                                                        | view2::child( "task" )
                                                        | view2::alias_src( t2 )
                                                        | act2::fetch_node( kmap ) );

                            REQUIRE( t2 == nw->resolve( dlt ) );
                        }

                        WHEN( "activate.task <t1>" )
                        {
                            REQUIRE_TRY( cli->parse_raw( ":@/task.t1" ) );
                            REQUIRE_TRY( cli->parse_raw( ":activate.task" ) );

                            THEN( "both active tasks are pushed to daily log" )
                            {
                                REQUIRE(( view2::log::daily_log_root
                                        | view2::direct_desc( present_date_string() )
                                        | view2::child( "task" )
                                        | view2::all_of( view2::alias_src
                                                       , { view2::task::task_root | view2::direct_desc( "t1" ) 
                                                         , view2::task::task_root | view2::direct_desc( "t2" ) } )
                                        | act2::exists( kmap ) ));
                            }
                        }

                        WHEN( "erase daily log" )
                        {
                            REQUIRE_TRY( view2::log::daily_log_root | act2::erase_node( kmap ) );

                            WHEN( "activate.task <t1>" )
                            {
                                REQUIRE_TRY( cli->parse_raw( ":@/task.t1" ) );
                                REQUIRE_TRY( cli->parse_raw( ":activate.task" ) );

                                THEN( "both active tasks are pushed to daily log" )
                                {
                                    REQUIRE(( view2::log::daily_log_root
                                            | view2::direct_desc( present_date_string() )
                                            | view2::child( "task" )
                                            | view2::all_of( view2::alias_src
                                                           , { view2::task::task_root | view2::direct_desc( "t1" ) 
                                                             , view2::task::task_root | view2::direct_desc( "t2" ) } )
                                            | act2::exists( kmap ) ));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    GIVEN( "one active task <t1>" )
    {
        REQUIRE_RES( cli->parse_raw( ":create.task t1" ) );
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
                    auto const task = REQUIRE_TRY( view2::task::task_root
                                                 | view2::child( "t1" )
                                                 | act2::fetch_node( kmap ) );
                    auto const dlt = REQUIRE_TRY( anchor::node( dl )
                                                | view2::child( "task" )
                                                | view2::alias
                                                | act2::fetch_node( kmap ) );

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
    //             auto const task = REQUIRE_TRY( view2::task::task_root
    //                                          | view2::child( "pass_test" )
    //                                          | act2::fetch_node( kmap ) );
    //             auto const dlt = REQUIRE_TRY( anchor::node( dl )
    //                                         | view2::child( "task" )
    //                                         | view2::alias
    //                                         | act2::fetch_node( kmap ) );

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
