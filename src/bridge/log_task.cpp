/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cmd/log.hpp"
#include "component.hpp"
#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "event/event.hpp"
#include "event/event_clerk.hpp"
#include "kmap.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"
#include "tag/tag.hpp"
#include "task/task.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <emscripten/bind.h>

namespace kmap::bridge {

// TODO: So... the gist here is that there is Log and Task, and there's no interdependency between them, though they do publish events about their doings.
//       Rather, there's a 3rd party that performs the interplay, "bridges" them. That is what this is trying to achieve. It consumes the events and dispatches
//       to each. Maybe the "bridge" concept isn't even the right approach. Maybe individual outlets are the way to go, but that is what is ultimately installed.
//       The bridge is just a place to gather the functionality into a file.
//       Part of the gist is avoiding needing to alter Kmap.hpp/.cpp code to be aware of this component. An idea is to use something like the command registration.
//       The other requirement is for dependencies. Basically, this has other component dependencies. If that can somehow be communicated, then we won't hit the continual
//       uninitialized dep problem.
// TODO: A thought on how to achieve dependency resolution is through event system. Basically, when each component is initialized, it fires an event notifying its init-completion.
//       Other dependent components await requisite event notifications. For the possibility that a component isn't listening before a component fires init'd, some kind of
//       state would need to be maintained: if requisites already fired, init this component, otherwise, wait until all requisites fired.
//       This would allow for a dynamic means of handling component dependencies. It would also mean that _at least_ `EventStore` would need to be a common dependency.
//       I.e., EventStore is initialized before dynamic components.
//       A component might look something like: if req components fired, then init, else, create outlet for reqs that will call this component to be initialized.
//       It's a little complicated, because it's not like all req components are fired at the same time; however, it may be possible to make it work:
//       An outlet that listens for { "subject.component", "verb.commissioned" }, so any init'd fired will trigger. (following: { "verb.decommissioned" } on dtor)
//       Assuming I can integrate the payload mechanism, the payload can contain the ID of the component intialized, and store it somewhere, in a component store of some sort.
//       Then, each component can query this store on creation to see if it can be initialized at the outset, or listen for { "subject.component", "verb.commissioned" }
//       to inspect the component store for requisites. Outlet for ComponentStore must precede component outlets.
struct LogTask : ComponentBase
{
    Kmap& kmap_;
    event::EventClerk eclerk_;

    LogTask( Kmap& kmap )
        : kmap_{ kmap }
        , eclerk_{ kmap }
    {
    }
    virtual ~LogTask() = default;

    auto initialize()
        -> Result< void > override
    {
        auto rv = KMAP_MAKE_RESULT( void );

        fmt::print( "ltbridge :: initialize\n" );

        KTRY( install_default_events() );

        rv = outcome::success();

        return rv;
    }

    auto install_default_events()
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        // Note: Under domain of "log", as no change is actually made to task, all changes made to log.
        KTRY( eclerk_.install_outlet( Leaf{ .heading = "log.add_task_on_task_opening"
                                          , .requisites = { "subject.task_store", "verb.opened", "object.task" }
                                          , .description = "adds task to task list when a task is opened"
                                          , .action = R"%%%(kmap.ltbridge().push_task_to_log();)%%%" } ) );
        KTRY( eclerk_.install_outlet( Leaf{ .heading = "log.add_open_tasks_on_daly_log_creation"
                                          , .requisites = { "subject.log", "verb.created", "object.daily" }
                                          , .description = "adds open tasks to task list when new daily log is created"
                                          , .action = R"%%%(kmap.ltbridge().push_open_tasks_to_log();)%%%" } ) );
        
        rv = outcome::success();

        return rv;
    }

    auto push_task_to_log()
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );
        KMAP_LOG_LINE();

        auto const payload = KTRY( kmap_.event_store().fetch_payload() );
        auto const task = KTRY( uuid_from_string( payload ) ); BC_ASSERT( kmap_.exists( task ) );
        auto const dl = KTRY( cmd::log::fetch_or_create_daily_log( kmap_ ) );

        KTRY( view::make( dl )
            | view::child( "task" )
            | view::alias( view::Alias::Src{ task } )
            | view::fetch_or_create_node( kmap_ ) );

        rv = outcome::success();

        return rv;
    }

    auto push_open_tasks_to_log()
        -> Result< void >
    {
        auto const open_tag = KTRY( fetch_tag( kmap_, "status.open" ) ); // TODO: Possible to be ambiguous. If another "status.open'tag" exists, would return ambiguous.
        auto const has_open_tag = [ & ]( Uuid const& n )
        {
            return has_tag( kmap_, n, open_tag );
        };
        auto rv = KMAP_MAKE_RESULT( void );
        auto const task_root = KTRY( fetch_task_root( kmap_ ) );
        auto const open_tasks = view::make( task_root )
                              | view::child( view::PredFn{ has_open_tag } )
                              | view::to_node_set( kmap_ )
                              | act::order( kmap_ );
        auto const dl = KTRY( cmd::log::fetch_or_create_daily_log( kmap_ ) );

fmt::print( "open_tasks.size(): {}\n", open_tasks.size() );
        for( auto const& open_task : open_tasks )
        {
            KTRY( view::make( dl )
                | view::child( "task" )
                | view::alias( view::Alias::Src{ open_task } ) // TODO: Why not view::alias( open_tasks [UuidVec]? )
                | view::fetch_or_create_node( kmap_ ) );
        }
        
        rv = outcome::success();

        return rv;
    }
};

SCENARIO( "push open tasks to log", "[cmd][log][task]" )
{
    KMAP_COMMAND_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    
    GIVEN( "no tasks" )
    {
        GIVEN( "no daily log" )
        {
            WHEN( "create.daily.log" )
            {
                REQUIRE_RES( cli.parse_raw( ":create.daily.log" ) );

                THEN( "no log tasks" )
                {
                    auto const dl = REQUIRE_TRY( cmd::log::fetch_daily_log( kmap ) );
                    
                    REQUIRE( !( view::make( dl )
                              | view::child( "task" )
                              | view::exists( kmap ) ) );
                }
                WHEN( "create.task" )
                {
                    REQUIRE_RES( cli.parse_raw( ":create.task Pass Test" ) );

                    THEN( "task pushed to daily log" )
                    {
                        auto const dl = REQUIRE_TRY( cmd::log::fetch_daily_log( kmap ) );
                        auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
                        auto const task = REQUIRE_TRY( view::make( troot )
                                                    | view::child( "pass_test" )
                                                    | view::fetch_node( kmap ) );
                        auto const dlt = REQUIRE_TRY( view::make( dl )
                                                    | view::child( "task" )
                                                    | view::alias
                                                    | view::fetch_node( kmap ) );

                        REQUIRE( task == kmap.resolve( dlt ) );
                    }
                }
            }
            WHEN( "create.task" )
            {
                REQUIRE_RES( cli.parse_raw( ":create.task Pass Test" ) );

                THEN( "task pushed to daily log" )
                {
                    auto const dl = REQUIRE_TRY( cmd::log::fetch_daily_log( kmap ) );
                    auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
                    auto const task = REQUIRE_TRY( view::make( troot )
                                                 | view::child( "pass_test" )
                                                 | view::fetch_node( kmap ) );
                    auto const dlt = REQUIRE_TRY( view::make( dl )
                                                | view::child( "task" )
                                                | view::alias
                                                | view::fetch_node( kmap ) );

                    REQUIRE( task == kmap.resolve( dlt ) );
                }
            }
        }
    }
    GIVEN( "one open task" )
    {
        REQUIRE_RES( cli.parse_raw( ":create.task Pass Test" ) );
        {
            auto const ln = REQUIRE_TRY( cmd::log::fetch_daily_log( kmap ) );
            REQUIRE_RES( kmap.erase_node( ln ) );
        }

        GIVEN( "no daily log" )
        {
            WHEN( "create.daily.log" )
            {
                REQUIRE_RES( cli.parse_raw( ":create.daily.log" ) );

                THEN( "open task pushed to daily log" )
                {
                    auto const dl = REQUIRE_TRY( cmd::log::fetch_daily_log( kmap ) );
                    auto const troot = REQUIRE_TRY( fetch_task_root( kmap ) );
                    auto const task = REQUIRE_TRY( view::make( troot )
                                                 | view::child( "pass_test" )
                                                 | view::fetch_node( kmap ) );
                    auto const dlt = REQUIRE_TRY( view::make( dl )
                                                | view::child( "task" )
                                                | view::alias
                                                | view::fetch_node( kmap ) );

                    REQUIRE( task == kmap.resolve( dlt ) );
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

} // namespace kmap::bridge

namespace {

struct LTBridge
{
    kmap::bridge::LogTask ltbridge;

    LTBridge( kmap::Kmap& kmap )
        : ltbridge{ kmap }
    {

    }

    auto push_task_to_log()
        -> kmap::binding::Result< void >
    {
        return ltbridge.push_task_to_log();
    }
    auto push_open_tasks_to_log()
        -> kmap::binding::Result< void >
    {
        return ltbridge.push_open_tasks_to_log();
    }
};

auto ltbridge()
    -> LTBridge
{
    return LTBridge{ kmap::Singleton::instance() };
}

} // namespace anonymous

using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_ltbridge )
{
    function( "ltbridge", &::ltbridge );

    class_< LTBridge >( "LTBridge" )
        .function( "push_task_to_log", &::LTBridge::push_task_to_log )
        .function( "push_open_tasks_to_log", &::LTBridge::push_open_tasks_to_log )
        ;
}

namespace {
namespace log_open_tasks_on_create_daily_log_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    log_open_tasks_on_daily_log_created 
,   "creates aliases, at `<log>.task`, to `open'tag` tasks when log.daily is created"
,   std::set({ "task_store"s, "log_store"s })
,   kmap::bridge::LogTask
);

} // namespace create_subtask_def 
} // namespace anonymous