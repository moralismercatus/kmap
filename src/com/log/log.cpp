/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/cli/cli.hpp" // TODO: To be replaced by command.hpp once transition to CommandStore is complete.
#include "com/cmd/cclerk.hpp"
#include "com/cmd/command.hpp"
#include "com/event/event.hpp"
#include "com/event/event_clerk.hpp"
#include "component.hpp"
#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "kmap.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"
#include "com/tag/tag.hpp"
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
struct LogStore : public Component
{
    static constexpr auto id = "log_store";
    constexpr auto name() const -> std::string_view override { return id; }

    com::EventClerk eclerk_;
    CommandClerk cclerk_;

    LogStore( kmap::Kmap& kmap
            , std::set< std::string > const& requisites
            , std::string const& description )
        : Component{ kmap, requisites, description }
        , eclerk_{ kmap }
        , cclerk_{ kmap }
    {
        register_standard_commands();
    }
    virtual ~LogStore() = default;

    auto initialize()
        -> Result< void > override
    {
        auto rv = KMAP_MAKE_RESULT( void );

        fmt::print( "log_store :: initialize\n" );

        KTRY( cclerk_.install_registered() );

        rv = outcome::success();

        return rv;
    }

    auto load()
        -> Result< void > override
    {
        auto rv = KMAP_MAKE_RESULT( void );

        KTRY( cclerk_.check_registered() );

        rv = outcome::success();

        return rv;
    }

    auto register_standard_commands()
        -> void
    {
        // auto const cli = KTRY( fetch_component< com::Cli >() );

        // create.daily.log
        {
            auto const guard_code =
            R"%%%(
                return kmap.success( 'success' );
            )%%%";
            auto const action_code =
            R"%%%(
                let rv = null;
                const path = kmap.present_daily_log_path();
                const fln = kmap.fetch_descendant( kmap.root_node(), path ); // TODO: direct_desc?

                if( fln.has_value() )
                {
                    kmap.select_node( fln.value() );

                    rv = kmap.success( 'log already existent' );
                }
                else
                {
                    const cln = kmap.create_descendant( kmap.root_node(), path );

                    if( cln.has_value() )
                    {
                        kmap.select_node( cln.value() );

                        kmap.event_store().fire_event( to_VectorString( [ 'subject.log', 'verb.created', 'object.daily' ] ) ).throw_on_error();

                        rv = kmap.success( 'log created' );
                    }
                    else
                    {
                        rv = kmap.failure( cln.error_message() );
                    }
                }

                return rv;
            )%%%";

            using Guard = com::Command::Guard;
            using Argument = com::Command::Argument;

            auto const description = "creates a log for today's date";
            auto const arguments = std::vector< Argument >{};
            auto const guard = Guard{ "unconditional", guard_code };
            auto const action = action_code;
            auto const cmd = com::Command{ .path = "create.daily.log"
                                         , .description = description
                                         , .arguments = arguments
                                         , .guard = guard
                                         , .action = action };

            cclerk_.register_command( cmd );
        }
        // select.daily.log
        {
            auto const guard_code =
            R"%%%(
                return kmap.success( 'success' );
            )%%%";
            auto const action_code =
            R"%%%(
                let rv = null;
                const path = kmap.present_daily_log_path();
                const ln = kmap.fetch_node( path );

                if( ln.has_value() )
                {
                    kmap.select_node( ln.value() );

                    rv = kmap.success( 'today\'s log selected' );
                }
                else
                {
                    rv = kmap.failure( ln.error_message() );
                }

                return rv;
            )%%%";

            using Guard = com::Command::Guard;
            using Argument = com::Command::Argument;

            auto const description = "selects the log for today's date";
            auto const arguments = std::vector< Argument >{};
            auto const guard = Guard{ "unconditional", guard_code };
            auto const action = action_code;
            auto const cmd = com::Command{ .path = "select.daily.log"
                                         , .description = description
                                         , .arguments = arguments
                                         , .guard = guard
                                         , .action = action };

            cclerk_.register_command( cmd );
        }
        // log.reference
        {
            auto const guard_code =
            R"%%%(
                return kmap.success( 'success' );
            )%%%";
            auto const action_code =
            R"%%%(
                let rv = null;
                const path = kmap.present_daily_log_path();
                const ln = kmap.fetch_or_create_node( kmap.root_node(), path );

                if( ln.has_value() )
                {
                    const sel = kmap.selected_node();
                    const ref = kmap.create_reference( sel, ln.value() );

                    if( ref.has_value() )
                    {
                        rv = kmap.success( 'reference logged' );
                    }
                    else
                    {
                        rv = kmap.failure( ref.error_message() );
                    }
                }
                else
                {
                    rv = kmap.failure( ln.error_message() );
                }

                return rv;
            )%%%";

            using Guard = com::Command::Guard;
            using Argument = com::Command::Argument;

            auto const description = "creates alias for the selected node as reference of the daily log";
            auto const arguments = std::vector< Argument >{};
            auto const guard = Guard{ "unconditional", guard_code };
            auto const action = action_code;
            auto const cmd = com::Command{ .path = "log.reference"
                                         , .description = description
                                         , .arguments = arguments
                                         , .guard = guard
                                         , .action = action };

            cclerk_.register_command( cmd );
        }
    }
};

auto present_date_string()
    -> std::string
{
    // TODO: Awaiting C++20 chrono date capabilities.
    // return to_log_date_string( std::chrono::system_clock::now() );

    auto t = std::time( nullptr );
    auto ss = std::stringstream{};

    ss << std::put_time( std::localtime( &t ), "%Y.%m.%d" );

    return ss.str();
}

auto present_daily_log_path()
    -> std::string
{
    auto const pds = present_date_string();

    return fmt::format( "{}.{}"
                      , "log.daily" 
                      , pds );
}

auto fetch_or_create_daily_log( Kmap& kmap
                              , std::string const& date )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( view::make( kmap.root_node_id() )
             | view::direct_desc( present_daily_log_path() )
             | view::fetch_or_create_node( kmap ) );

    return rv;
}

auto fetch_or_create_daily_log( Kmap& kmap )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( fetch_or_create_daily_log( kmap, present_daily_log_path() ) );

    return rv;
}

auto fetch_daily_log( Kmap const& km
                    , std::string const& date )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( km.root_view()
             | view::direct_desc( date )
             | view::fetch_node( km ) );

    return rv;
}

auto fetch_daily_log( Kmap const& kmap )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( fetch_daily_log( kmap, present_daily_log_path() ) );

    return rv;
}

namespace binding {

using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_log )
{
    function( "present_date_string", &kmap::com::present_date_string );
    function( "present_daily_log_path", &kmap::com::present_daily_log_path );
}

} // namespace binding

} // namespace kmap::com

namespace {
namespace log_store_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::LogStore
,   std::set({ "command_store"s, "event_store"s, "tag_store"s })
,   "responsible for logs"
);

} // namespace log_store_def 
} // namespace anonymous

using namespace emscripten;

// EMSCRIPTEN_BINDINGS( kmap_com_log_store )
// {
//     function( "log_store", &::log_store );

//     class_< LT >( "LogStore" )
//         .function( "push_task_to_log", &::LT::push_task_to_log )
//         .function( "push_open_tasks_to_log", &::LT::push_open_tasks_to_log )
//         ;
// }