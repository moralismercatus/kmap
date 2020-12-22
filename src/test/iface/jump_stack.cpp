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

BOOST_AUTO_TEST_SUITE( kmap_iface )
// TODO:
/******************************************************************************/
// BOOST_AUTO_TEST_SUITE( jump_stack
//                      ,
//                      * utf::depends_on( "kmap_iface/create_node" ) )
//                     //  * utf::depends_on( "kmap_iface/select_node" ) )
// BOOST_AUTO_TEST_CASE( jump_out
//                     ,
//                     * utf::fixture< ClearMapFixture >() )
// {
//     auto& kmap = Singleton::instance();
//     auto const nodes = kmap.node_fetcher();

//     create_lineages( "1"
//                    , "2"
//                    , "3" );
    
//     kmap.jump_to( nodes[ "1" ] );
//     kmap.jump_to( nodes[ "2" ] );
//     kmap.jump_to( nodes[ "3" ] );

//     auto const jo1 = kmap.jump_out();
//     BOOST_TEST_REQUIRE( jo1.has_value() );
//     BOOST_TEST( *jo1 == nodes[ "2" ] );
//     BOOST_TEST( kmap.selected_node() == nodes[ "2" ] );

//     auto const jo2 = kmap.jump_out();
//     BOOST_TEST_REQUIRE( jo2.has_value() );
//     BOOST_TEST( *jo2 == nodes[ "1" ] );
//     BOOST_TEST( kmap.selected_node() == nodes[ "1" ] );

//     auto const jo3 = kmap.jump_out();
//     BOOST_TEST_REQUIRE( jo3.has_value() );
//     BOOST_TEST( *jo3 == nodes[ "root" ] );
//     BOOST_TEST( kmap.selected_node() == nodes[ "root" ] );

//     auto const jo4 = kmap.jump_out();
//     BOOST_TEST( !jo4.has_value() );
//     BOOST_TEST( kmap.selected_node() == nodes[ "root" ] );
// }
// BOOST_AUTO_TEST_CASE( jump_in
//                     ,
//                     * utf::fixture< ClearMapFixture >() )
// {
//     // TODO
// }
// BOOST_AUTO_TEST_CASE( jump_out_in
//                     ,
//                     * utf::fixture< ClearMapFixture >() )
// {
//     // TODO
// }
// BOOST_AUTO_TEST_SUITE_END( /* jump_stack */ )
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test