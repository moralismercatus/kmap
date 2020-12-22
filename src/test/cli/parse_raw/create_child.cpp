/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../master.hpp"

#include "../../../contract.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( cli )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( parse_raw )

BOOST_AUTO_TEST_CASE( create_child 
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    kmap.select_node( kmap.root_node_id() );
    BOOST_TEST( cli.parse_raw( ":create.child 1" ).result == succ );
    kmap.select_node( kmap.root_node_id() );
    BOOST_TEST( cli.parse_raw( ":create.child 1" ).result == fail );
}

BOOST_AUTO_TEST_CASE( create_sibling
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    create_lineages( "1"
                   , "1.2" );

    BOOST_TEST( cli.parse_raw( ":create.sibling 1" ).result == fail );

    kmap.select_node( nodes[ "/1" ] );

    BOOST_TEST( cli.parse_raw( ":create.sibling 2" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":create.sibling 2" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.sibling 3" ).result == succ );
}

BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test