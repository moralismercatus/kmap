/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/event/event.hpp"
#include "com/event/event_clerk.hpp"
#include "com/network/network.hpp"
#include "com/log/log.hpp"
#include "com/tag/tag.hpp"
#include "com/task/task.hpp"
#include "component.hpp"
#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "kmap.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <emscripten/bind.h>

namespace kmap::com {

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
struct LogTask : public Component
{
    static constexpr auto id = "log_task";
    constexpr auto name() const -> std::string_view override { return id; }

    com::EventClerk eclerk_;

    LogTask( Kmap& kmap
           , std::set< std::string > const& requisites
           , std::string const& description )
        : Component{ kmap, requisites, description }
        , eclerk_{ kmap }
    {
        KTRYE( register_standard_events() );
    }
    virtual ~LogTask() = default;

    auto initialize()
        -> Result< void > override
    {
        auto rv = KMAP_MAKE_RESULT( void );

        fmt::print( "log_task :: initialize\n" );

        KTRY( eclerk_.install_registered() );

        rv = outcome::success();

        return rv;
    }

    auto load()
        -> Result< void > override
    {
        auto rv = KMAP_MAKE_RESULT( void );

        KTRY( eclerk_.check_registered() );

        rv = outcome::success();

        return rv;
    }

    auto register_standard_events()
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        // Note: Under domain of "log", as no change is actually made to task, all changes made to log.
        eclerk_.register_outlet( Leaf{ .heading = "log.add_task_on_task_opening"
                                     , .requisites = { "subject.task_store", "verb.opened", "object.task" }
                                     , .description = "adds task to log task list when a task is opened"
                                     , .action = R"%%%(kmap.log_task().push_task_to_log();)%%%" } );
        eclerk_.register_outlet( Leaf{ .heading = "log.add_open_tasks_on_daly_log_creation"
                                     , .requisites = { "subject.log", "verb.created", "object.daily" }
                                     , .description = "adds open tasks to task list when new daily log is created"
                                     , .action = R"%%%(kmap.log_task().push_open_tasks_to_log();)%%%" } );
        
        rv = outcome::success();

        return rv;
    }

    auto push_task_to_log()
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        auto& kmap = kmap_inst();
        auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
        auto const nw = KTRY( kmap.fetch_component< com::Network >() );
        auto const payload = KTRY( estore->fetch_payload() );
        auto const task = KTRY( uuid_from_string( payload ) ); BC_ASSERT( nw->exists( task ) );
        auto const dl = KTRY( com::fetch_or_create_daily_log( kmap ) );

        KTRY( view::make( dl )
            | view::child( "task" )
            | view::alias( view::Alias::Src{ task } )
            | view::fetch_or_create_node( kmap ) );

        rv = outcome::success();

        return rv;
    }

    auto push_open_tasks_to_log()
        -> Result< void >
    {
        auto& km = kmap_inst();
        auto const tag_store = KTRY( fetch_component< TagStore >() );
        auto const open_tag = KTRY( tag_store->fetch_tag( "status.open" ) ); // TODO: Possible to be ambiguous. If another "status.open'tag" exists, would return ambiguous.
        auto const has_open_tag = [ & ]( Uuid const& n )
        {
            return tag_store->has_tag( n, open_tag );
        };
        auto rv = KMAP_MAKE_RESULT( void );
        auto const task_root = KTRY( fetch_task_root( km ) );
        auto const open_tasks = view::make( task_root )
                              | view::child( view::PredFn{ has_open_tag } )
                              | view::to_node_set( km )
                              | act::order( km );
        auto const dl = KTRY( com::fetch_or_create_daily_log( km ) );

fmt::print( "open_tasks.size(): {}\n", open_tasks.size() );
        for( auto const& open_task : open_tasks )
        {
            KTRY( view::make( dl )
                | view::child( "task" )
                | view::alias( view::Alias::Src{ open_task } ) // TODO: Why not view::alias( open_tasks [UuidVec]? )
                | view::fetch_or_create_node( km ) );
        }
        
        rv = outcome::success();

        return rv;
    }
};

SCENARIO( "push open tasks to log", "[cmd][log][task]" )
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
            WHEN( "create.task" )
            {
                REQUIRE_RES( cli->parse_raw( ":create.task Pass Test" ) );

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

                WHEN( "create.subtask" )
                {
                    REQUIRE_RES( cli->parse_raw( ":create.subtask Subtask" ) );

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
    GIVEN( "one open task" )
    {
        REQUIRE_RES( cli->parse_raw( ":create.task Pass Test" ) );
        {
            auto const ln = REQUIRE_TRY( com::fetch_daily_log( kmap ) );
            REQUIRE_RES( nw->erase_node( ln ) );
        }

        GIVEN( "no daily log" )
        {
            WHEN( "create.daily.log" )
            {
                REQUIRE_RES( cli->parse_raw( ":create.daily.log" ) );

                THEN( "open task pushed to daily log" )
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

struct LT
{ 
    kmap::Kmap& km;

    LT( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto push_task_to_log()
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::LogTask >() )->push_task_to_log();
    }
    auto push_open_tasks_to_log()
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::LogTask >() )->push_open_tasks_to_log();
    }
};

auto log_task()
    -> LT
{
    return LT{ kmap::Singleton::instance() };
}

} // namespace anonymous

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

using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_com_log_task )
{
    function( "log_task", &::log_task );

    class_< LT >( "LogTask" )
        .function( "push_task_to_log", &::LT::push_task_to_log )
        .function( "push_open_tasks_to_log", &::LT::push_open_tasks_to_log )
        ;
}