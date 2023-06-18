/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/network/command.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "utility.hpp"
#include "test/util.hpp"
#include "com/cli/cli.hpp"
#include "path/act/select_node.hpp"

#include <catch2/catch_test_macros.hpp>

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
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto NetworkCommand::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    
    KTRY( cclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}

auto NetworkCommand::register_standard_commands()
    -> void
{
    KM_RESULT_PROLOG();

    // count.descendants
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const selected = nw.selected_node();
        const count = ktry( kmap.count_descendants( selected ) );

        console.log( 'descendant count: ' + count ); // TODO: kconsole.log
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "tally all descendant nodes from selected";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "count.descendants"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // create.alias
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const src = ktry( kmap.fetch_node( args.get( 0 ) ) );
        const c = ktry( kmap.create_alias( src, nw.selected_node() ) );

        ktry( kmap.select_node( c ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "creates alias child node";
        auto const arguments = std::vector< Argument >{ Argument{ "destination_path", "path to destination node", "heading_path" } };
        auto const command = Command{ .path = "create.alias"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // create.child
    {
        auto const action_code =
        R"%%%(
        const title = args.get( 0 );
        const nw = kmap.network();
        const child = ktry( nw.create_child( kmap.network().selected_node(), title ) );

        ktry( nw.select_node( child ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "creates child node";
        auto const arguments = std::vector< Argument >{ Argument{ "title", "title", "unconditional" } };
        auto const command = Command{ .path = "create.child"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // create.sibling
    {
        auto const action_code =
        R"%%%(
        const title = args.get( 0 );
        const nw = kmap.network();
        const selected = nw.selected_node();
        const parent = ktry( nw.fetch_parent( selected ) );
        const child = ktry( nw.create_child( parent, title ) );

        ktry( nw.select_node( child ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "creates sibling node";
        auto const arguments = std::vector< Argument >{ Argument{ "title", "title", "unconditional" } };
        auto const command = Command{ .path = "create.sibling"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // erase.node
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const selected = nw.selected_node();

        ktry( nw.erase_node( selected ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "moves selected node to destination";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "erase.node" // TODO: erase as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // move.children
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const src = nw.selected_node();
        const dst = ktry( nw.fetch_node( args.get( 0 ) ) );

        ktry( nw.move_children( src, dst ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "moves children of selected node to destination";
        auto const arguments = std::vector< Argument >{ Argument{ "destination_path", "path to destination node", "heading_path" } };
        auto const command = Command{ .path = "move.children" // TODO: erase as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // move.node
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const src = nw.selected_node();
        const dst = ktry( nw.fetch_node( args.get( 0 ) ) );

        ktry( nw.move_node( src, dst ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "moves node to destination path";
        auto const arguments = std::vector< Argument >{ Argument{ "destination_path", "path to destination node", "heading_path" } };
        auto const command = Command{ .path = "move.node" // TODO: erase as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // move.up
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const sel = nw.selected_node();
        const other = ktry( kmap.fetch_above( sel ) );

        ktry( kmap.swap_nodes( sel, other ) );
        ktry( nw.select_node( sel ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "swaps node with node above";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "move.up" // TODO: move.node.up with move.up as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // move.down
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const sel = nw.selected_node();
        const other = ktry( kmap.fetch_below( sel ) );

        ktry( kmap.swap_nodes( sel, other ) );
        ktry( nw.select_node( sel ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "swaps node with node below";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "move.down" // TODO: move.node.down with move.down as alias.
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // print.id
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const selected = nw.selected_node();
        const id = kmap.uuid_to_string( selected );

        console.log( 'id: ' + id ); // TODO: kconsole.log
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "prints uuid for node";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "print.id"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // resolve.alias
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();
        const selected = nw.selected_node();

        ktry( kmap.select_node( kmap.resolve_alias( selected ) ) );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "selects underlying source node for selected alias";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "resolve.alias"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // travel.left
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();

        ktry( nw.travel_left() );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "selects parent";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "travel.left"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // travel.right
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();

        ktry( nw.travel_right() );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "selects child";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "travel.right"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // travel.up
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();

        ktry( nw.travel_up() );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "selects sibling directly above";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "travel.up"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
    // travel.down
    {
        auto const action_code =
        R"%%%(
        const nw = kmap.network();

        ktry( nw.travel_down() );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "selects sibling directly below";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "travel.down"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
}

SCENARIO( "count.descendants", "[network][cmd]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network.command", "cli" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );
    auto const root = nw->root_node();

    GIVEN( "/1.2" )
    {
        REQUIRE_RES(( view::make( root )
                    | view::direct_desc( "1.2" )
                    | view::create_node( km ) ));

        GIVEN( "select.node /1" )
        {
        REQUIRE_RES(( view::make( root )
                    | view::child( "1" )
                    | act::select_node( km ) ));

            WHEN( ":count.descendants" )
            {
                REQUIRE_TRY( cli->parse_raw( ":count.descendants" ) );

                THEN( "displays 1" )
                {
                    // TODO: Need to fetch display.
                    // One way that I can "work around" is to only display the executed command if it's empty (been cleared).
                    // That way, count.descendants can manually print to the popup.
                    // BUT HONESTLY, ideally, the CLI would function more like a shell and could simply print outputs to shell. Like console.log...
                    // REQUIRE( cli->fetch_input == "1" );
                }
            }
        }
    }
}

SCENARIO( "resolve.alias", "[network][cmd][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network.command", "cli" );
    auto& km = Singleton::instance();
    auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const rootn = nw->root_node();

    GIVEN( "/1, /2.1[/1]" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( rootn, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( rootn, "2" ) );
        auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );
        THEN( "`:resolve.alias <selected:/2.1>` resolves to /1" )
        {
            REQUIRE_TRY( nw->select_node( a21 ) );
            REQUIRE( nw->selected_node() == a21 );
            REQUIRE_TRY( cli->parse_raw( ":resolve.alias" ) );
            REQUIRE( nw->selected_node() == n1 );
        }
        WHEN( "`:resolve.alias <selected /2>` fails" )
        {
            REQUIRE_TRY( nw->select_node( n2 ) );
            REQUIRE( nw->selected_node() == n2 );
            REQUIRE_TRY( cli->parse_raw( ":resolve.alias" ) ); // TODO: Should resolve.alias guard fail if not an alias, or "resolve" like Network::resolve does?
            REQUIRE( nw->selected_node() == n2 );
        }
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
} // namespace anon

} // namespace kmap
