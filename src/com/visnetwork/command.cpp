/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/cmd/cclerk.hpp>
#include <com/network/network.hpp>
#include <com/option/option.hpp>
#include <common.hpp>
#include <component.hpp>

// <test>
#include <com/cli/cli.hpp>
#include <test/util.hpp>
// </test>

#include <catch2/catch_test_macros.hpp>

namespace kmap::com {

class VisualNetworkCommand : public Component
{
    CommandClerk cclerk_;

public:
    static constexpr auto id = "visnetwork.command";
    constexpr auto name() const -> std::string_view override { return id; }

    VisualNetworkCommand( Kmap& km
                        , std::set< std::string > const& requisites
                        , std::string const& description )
        : Component{ km, requisites, description }
        , cclerk_{ km }
    {
        KM_RESULT_PROLOG();

        KTRYE( register_standard_commands() );
    }
    virtual ~VisualNetworkCommand() = default;

    auto initialize()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( void );

        KTRY( cclerk_.install_registered() );

        rv = outcome::success();

        return rv;
    }
    auto load()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( void );

        KTRY( cclerk_.check_registered() );

        rv = outcome::success();

        return rv;
    }

    auto register_standard_commands()
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = result::make_result< void >();

        // attach.image
        { 
            // TODO: ```url<url>```? That is, surround url in ```url```?
            auto const action_code =
            R"%%%(
                const url = args.get( 0 );
                const nw = kmap.network();
                const selected = nw.selected_node();
                const image_attrn = ktry( kmap.create_descendant( nw.selected_node(), ".$.node.image" ) );

                ktry( nw.update_body( image_attrn, url ) );
                ktry( nw.select_node( selected ) );
            )%%%";

            using Argument = com::Command::Argument;

            auto const description = "sets image for selected visual network node";
            auto const arguments = std::vector< Argument >{ Argument{ "url", "url", "unconditional" } }; // TODO: Should actually reference "url" argument for proper vetting.
            auto const command = Command{ .path = "attach.image"
                                        , .description = description
                                        , .arguments = arguments
                                        , .guard = "unconditional"
                                        , .action = action_code };

            KTRY( cclerk_.register_command( command ) );
        }
        // detach.image
        {
            auto const action_code =
            R"%%%(
                const url = args.get( 0 );
                const nw = kmap.network();
                const selected = nw.selected_node();
                const image_attrn = ktry( nw.fetch_node( nw.selected_node(), ".$.node.image" ) );

                ktry( nw.erase_node( image_attrn ) );
                ktry( nw.select_node( selected ) );
            )%%%";

            using Argument = com::Command::Argument;

            auto const description = "removes image for selected visual network node";
            auto const arguments = std::vector< Argument >{};
            auto const command = Command{ .path = "detach.image"
                                        , .description = description
                                        , .arguments = arguments
                                        , .guard = "unconditional"
                                        , .action = action_code };

            KTRY( cclerk_.register_command( command ) );
        }

        rv = outcome::success();

        return rv;
    }
    // detach.image
};

namespace {
namespace visnetwork_command_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::VisualNetworkCommand
,   std::set( { "network"s, "visnetwork"s, "command.store"s, "command.standard_items"s } )
,   "standard commands for visnetwork"
);

} // namespace option_store_command_def 
}
} // namespace kmap::com


