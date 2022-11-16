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
    register_standard_commands();
}

auto NetworkCommand::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto NetworkCommand::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    
    KTRY( cclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}

auto NetworkCommand::register_standard_commands()
    -> void
{
    // create.alias
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
        let rv = null;
        const nw = kmap.network();
        const src = kmap.fetch_node( args.get( 0 ) );

        if( src.has_value() )
        {
            const c = kmap.create_alias( src.value(), nw.selected_node() );

            if( c.has_value() )
            {
                kmap.select_node( nw.selected_node() ); // Refresh.

                rv = kmap.success( 'alias created' );
            }
            else
            {
                rv = kmap.failure( c.error_message() );
            }
        }
        else
        {
            rv = kmap.failure( src.error_message() );
        }

        return rv;
        )%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "creates alias child node";
        auto const arguments = std::vector< Argument >{ Argument{ "destination_path", "path to destination node", "heading_path" } };
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "create.alias"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        cclerk_.register_command( command );
    }
    // create.child
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
        const title = args.get( 0 );
        const nw = kmap.network();

        const child = nw.create_child( kmap.network().selected_node(), title );

        child.throw_on_error();

        nw.select_node( child.value() ).throw_on_error();

        rv = kmap.success( 'child created: ' + title );

        return rv;
        )%%%";

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

        cclerk_.register_command( command );
    }
    // create.sibling
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
        const title = args.get( 0 );
        const nw = kmap.network();
        const selected = nw.selected_node();
        const parent = nw.fetch_parent( selected );

        parent.throw_on_error();

        const child = nw.create_child( parent.value(), title );

        child.throw_on_error();

        nw.select_node( child.value() ).throw_on_error();

        rv = kmap.success( 'child created: ' + title );

        return rv;
        )%%%";

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

        cclerk_.register_command( command );
    }
    // erase.node
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const selected = nw.selected_node();

        nw.erase_node( selected ).throw_on_error();

        rv = kmap.success( 'node erased' );

        return rv;
        )%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "moves selected node to destination";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "erase.node" // TODO: erase as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        cclerk_.register_command( command );
    }
    // move.children
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const src = nw.selected_node();
        const dst = nw.fetch_node( args.get( 0 ) ); dst.throw_on_error();

        nw.move_children( src, dst.value() ).throw_on_error();

        rv = kmap.success( 'node erased' );

        return rv;
        )%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "moves children of selected node to destination";
        auto const arguments = std::vector< Argument >{ Argument{ "destination_path", "path to destination node", "heading_path" } };
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "move.children" // TODO: erase as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        cclerk_.register_command( command );
    }
    // move.node
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const src = nw.selected_node();
        const dst = nw.fetch_node( args.get( 0 ) ); dst.throw_on_error();

        nw.move_node( src, dst.value() ).throw_on_error();

        rv = kmap.success( 'node erased' );

        return rv;
        )%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "erases node";
        auto const arguments = std::vector< Argument >{ Argument{ "destination_path", "path to destination node", "heading_path" } };
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "move.node" // TODO: erase as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        cclerk_.register_command( command );
    }
    // print.id
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const selected = nw.selected_node();
        const id = kmap.uuid_to_string( selected );

        id.throw_on_error();

        rv = kmap.success( 'node id: ' + id.value() );

        return rv;
        )%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "prints uuid for node";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "print.id"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        cclerk_.register_command( command );
    }
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