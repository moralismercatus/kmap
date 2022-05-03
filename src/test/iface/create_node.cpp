/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

// TODO: I think we don't want this nested under kmap_iface; rather, we want kmap iface to be a dependency (depends_on()) where relevant.
BOOST_AUTO_TEST_SUITE( kmap_iface )

// TODO: Test negative cases to (those that result in ECs).
// TODO: Decide whether or not node creation belongs in "node_manip". I would say that it does.
BOOST_AUTO_TEST_CASE( create_node
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();

    BOOST_TEST( kmap.exists( "/" ) );
    BOOST_TEST( kmap.fetch_children( kmap.root_node_id() ).size() == 1 );

    auto const c1 = kmap.create_child( kmap.root_node_id()
                                     , "1" );
    BOOST_REQUIRE( c1 );
    BOOST_TEST( kmap.exists( c1.value() ) );
    BOOST_TEST( kmap.exists( "/1" ) );
    BOOST_TEST( kmap.fetch_children( kmap.root_node_id() ).size() == 2 );
    BOOST_TEST( kmap.selected_node() == kmap.root_node_id() );

    auto const c2 = kmap.create_child( kmap.root_node_id()
                                     , "2" );
    BOOST_REQUIRE( c2 );
    BOOST_TEST( kmap.exists( c2.value() ) );
    BOOST_TEST( kmap.exists( "/2" ) );
    BOOST_TEST( kmap.fetch_children( kmap.root_node_id() ).size() == 3 );

    auto const c3 = kmap.create_child( c1.value()
                                     , "3" );
    BOOST_REQUIRE( c3 );
    BOOST_TEST( kmap.exists( c3.value() ) );
    BOOST_TEST( kmap.exists( "/1.3" ) );
    BOOST_TEST( kmap.fetch_children( *kmap.fetch_leaf( "/1" ) ).size() == 1 );
}

BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test