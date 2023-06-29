/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/cmd/cclerk.hpp>
#include <com/option/option.hpp>
#include <common.hpp>
#include <component.hpp>

// <test>
#include <com/cli/cli.hpp>
#include <test/util.hpp>
// </test>

#include <catch2/catch_test_macros.hpp>

namespace kmap::com {

class OptionStoreCommand : public Component
{
    CommandClerk cclerk_;

public:
    static constexpr auto id = "option_store.command";
    constexpr auto name() const -> std::string_view override { return id; }

    OptionStoreCommand( Kmap& km
                      , std::set< std::string > const& requisites
                      , std::string const& description )
        : Component{ km, requisites, description }
        , cclerk_{ km }
    {
        KM_RESULT_PROLOG();

        KTRYE( register_standard_commands() );
    }
    virtual ~OptionStoreCommand() = default;

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

        // arg.option_path
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
                const root = kmap.fetch_node( "/meta.setting.option" );

                if( root.has_error() )
                {
                    return new kmap.VectorString();
                }
                else
                {
                    return kmap.complete_heading_path_from( root.value(), root.value(), arg );
                }
            )%%%";

            auto const description = "option heading path";
            
            KTRY( cclerk_.register_argument( com::Argument{ .path = "option_path"
                                                          , .description = description
                                                          , .guard = guard_code
                                                          , .completion = completion_code } ) );
        }
        // TODO: Ought not apply.option use selected_node, to be generally consistent...?
        // apply.option
        {
            auto const action_code =
            R"%%%(
                const ostore = kmap.option_store();
                const opt_root = ktry( kmap.fetch_node( "meta.setting.option"  ) );
                const target = ktry( kmap.fetch_descendant( opt_root, args.get( 0 ) ) );

                ktry( ostore.apply( target ) );
            )%%%";

            using Argument = com::Command::Argument;

            auto const description = "executes action for provided option";
            auto const arguments = std::vector< Argument >{ Argument{ "option_path"
                                                                    , "path to target option"
                                                                    , "option_path" }  };
            auto const command = Command{ .path = "apply.option"
                                        , .description = description
                                        , .arguments = arguments
                                        , .guard = "unconditional"
                                        , .action = action_code };

            KTRY( cclerk_.register_command( command ) );
        }

        rv = outcome::success();

        return rv;
    }
};

SCENARIO( ":apply.option accepts absolute and non-absolute path", "[option][cmd]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "option_store.command", "cli" );

    auto& km = kmap::Singleton::instance();
    auto const ostore = REQUIRE_TRY( km.fetch_component< com::OptionStore >() );
    auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );

    GIVEN( "option.1.2" )
    {
        REQUIRE_TRY( ostore->install_option( Option{ .heading = "1.2"
                                                   , .descr = "test_option_path"
                                                   , .value = "true"
                                                   , .action = "option_value.toString();" } ) );
        
        REQUIRE(( view2::option::option_root
                | view2::direct_desc( "1.2" )
                | act2::exists( km ) ));

        THEN( ":apply.option 1 fails" )
        {
            REQUIRE_RFAIL( cli->parse_raw( ":apply.option 1" ) );
        }
        THEN( ":apply.option 1.2 succeeds" )
        {
            REQUIRE_TRY( cli->parse_raw( ":apply.option 1.2" ) );
        }
        // THEN( ":apply.option 2 succeeds" )
        // {
        //     REQUIRE_TRY( cli->parse_raw( ":apply.option 2" ) );
        // }
    }
}

namespace {
namespace option_store_command_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::OptionStoreCommand
,   std::set({ "option_store"s, "command.store"s, "command.standard_items"s })
,   "standard commands for option_store"
);

} // namespace option_store_command_def 
}
} // namespace kmap::com


