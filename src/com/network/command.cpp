/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/network/command.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "utility.hpp"


namespace kmap::com {

NetworkCommand::NetworkCommand( Kmap& kmap
                              , std::set< std::string > const& requisites
                              , std::string const& description )
    : Component{ kmap, requisites, description }
    , cclerk_{ kmap }
{
}

auto NetworkCommand::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( install_commands() );

    rv = outcome::success();

    return rv;
}

auto NetworkCommand::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto NetworkCommand::install_commands()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    {
        auto const guard_code =
        R"%%%(```javascript
            return kmap.success( 'success' );
        ```)%%%";
        auto const action_code =
        R"%%%(```javascript
        const title = args.get( 0 );
        const nw = kmap.network();

        const child = nw.create_child( kmap.network().selected_node(), title );

        child.throw_on_error();

        nw.select_node( child.value() ).throw_on_error();

        rv = kmap.success( 'child created: ' + title );

        return rv;
        ```)%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "creates child node";
        auto const arguments = std::vector< Argument >{ Argument{ "title", "title", "unconditional" } };
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "create.child"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        KTRY( cclerk_.install_command( command ) );
    }
    {
        auto const guard_code =
        R"%%%(```javascript
            return kmap.success( 'success' );
        ```)%%%";
        auto const action_code =
        R"%%%(```javascript
        const title = args.get( 0 );
        const nw = kmap.network();
        const parent = nw.fetch_parent( nw.selected_node() );

        parent.throw_on_error();

        const child = nw.create_child( parent.value(), title );

        child.throw_on_error();

        nw.select_node( child.value() ).throw_on_error();

        rv = kmap.success( 'sibling created: ' + title );

        return rv;
        ```)%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "creates sibling node";
        auto const arguments = std::vector< Argument >{ Argument{ "title", "title", "unconditional" } };
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "create.sibling"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        KTRY( cclerk_.install_command( command ) );
    }

    rv = outcome::success();

    return rv;
}

namespace {
namespace network_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::NetworkCommand
,   std::set({ "command_store"s, "network"s })
,   "core commands for network"
);

} // namespace network_def 
}

} // namespace kmap
