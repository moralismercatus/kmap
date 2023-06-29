/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/visnetwork/option.hpp>

#include <contract.hpp>
#include <kmap.hpp>
#include <utility.hpp>
#include <test/util.hpp>
#include <com/cli/cli.hpp>
#include <path/act/select_node.hpp>

#include <catch2/catch_test_macros.hpp>

namespace kmap::com {

VisnetworkOption::VisnetworkOption( Kmap& kmap
                            , std::set< std::string > const& requisites
                            , std::string const& description )
    : Component{ kmap, requisites, description }
    , oclerk_{ kmap }
{
    KM_RESULT_PROLOG();

    KTRYE( register_standard_options() );
}

auto VisnetworkOption::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( oclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto VisnetworkOption::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    
    KTRY( oclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}

auto VisnetworkOption::register_standard_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( oclerk_.register_option( Option{ .heading = "canvas.network.background.color"
                                         , .descr = "Sets the background color for the nextwork pane."
                                         , .value = "\"#222222\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().network_pane() ) ).style.backgroundColor = option_value;" } ) );

    rv = outcome::success();

    return rv;
}

namespace {
namespace network_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::VisnetworkOption
,   std::set({ "option_store"s })
,   "core options for network"
);

} // namespace network_def 
} // namespace anon

} // namespace kmap
