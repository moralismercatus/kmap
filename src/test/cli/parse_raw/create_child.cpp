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
    auto const cli = KTRYE( kmap.fetch_component< com::Cli >() );

    BOOST_TEST( kmap.select_node( kmap.root_node_id() ) );
    BOOST_TEST( cli->parse_raw( ":create.child 1" ) );
    BOOST_TEST( kmap.select_node( kmap.root_node_id() ) );
    BOOST_TEST( cli->parse_raw( ":create.child 1" ) );
}

BOOST_AUTO_TEST_CASE( create_sibling
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const cli = KTRYE( kmap.fetch_component< com::Cli >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "1.2" );

    BOOST_TEST( cli->parse_raw( ":create.sibling 1" ) );

    BOOST_TEST( kmap.select_node( nodes[ "/1" ] ) );

    BOOST_TEST( cli->parse_raw( ":create.sibling 2" ) );
    BOOST_TEST( cli->parse_raw( ":create.sibling 2" ) );
    BOOST_TEST( cli->parse_raw( ":create.sibling 3" ) );
}

BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test