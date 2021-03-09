/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"

#include "../../canvas.hpp"
#include "../../network.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( network )

BOOST_AUTO_TEST_CASE( create_node )
{
    auto& kmap = Singleton::instance();
    auto nw = Network{ kmap.canvas().network_pane() };

    BOOST_TEST( nw.exists( nw.create_node( { 0x0 }, "0" ) ) );
    BOOST_TEST( nw.nodes().size() == 1 );
    BOOST_TEST( nw.exists( nw.create_node( { 0x1 }, "a" ) ) );
    BOOST_TEST( nw.nodes().size() == 2 );
    BOOST_TEST( nw.exists( nw.create_node( { 0x2 }, "b" ) ) );
    BOOST_TEST( nw.nodes().size() == 3 );
    BOOST_TEST( nw.exists( nw.create_node( { 0x3 }, "c" ) ) );
    BOOST_TEST( nw.nodes().size() == 4 );
}

BOOST_AUTO_TEST_CASE( add_edge
                    ,
                    * utf::depends_on( "network/create_node" ) )
{
    auto& kmap = Singleton::instance();
    auto nw = Network{ kmap.canvas().network_pane() };
 
    nw.create_node( { 0x0 }, "0" );
    nw.create_node( { 0x1 }, "a" );
    nw.create_node( { 0x2 }, "b" );
    nw.create_node( { 0x3 }, "c" );

    nw.add_edge( { 0x0 }, { 0x1 } );
    BOOST_TEST( nw.is_child( { 0x0 }, Uuid{ 0x1 } ) );
    nw.add_edge( { 0x1 }, { 0x2 } );
    BOOST_TEST( nw.is_child( { 0x1 }, Uuid{ 0x2 } ) );
    nw.add_edge( { 0x1 }, { 0x3 } );
    BOOST_TEST( nw.is_child( { 0x1 }, Uuid{ 0x3 } ) );
}

BOOST_AUTO_TEST_SUITE_END( /* network */ )

} // namespace kmap::test