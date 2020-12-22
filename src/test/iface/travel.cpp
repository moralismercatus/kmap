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
BOOST_AUTO_TEST_CASE( /*kmap_test/kmap_iface*/travel
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    // TODO: This should include checks that jump_stack is not being moved. As part of the travel spec., jump_stack is not supposed to be influenced.
    auto& kmap = Singleton::instance();
    auto& nw = kmap.network();
    auto const view = kmap.node_view();

    create_lineages( "new_root.a"
                   , "new_root.a.a_a"
                   , "new_root.a.a_b"
                   , "new_root.a.a_c"
                   , "new_root.b"
                   , "new_root.b.b_a"
                   , "new_root.b.b_b"
                   , "new_root.b.b_c"
                   , "new_root.c"
                   , "new_root.c.c_a"
                   , "new_root.c.c_b"
                   , "new_root.c.c_c" );

    auto const new_root = view[ "new_root" ];

    kmap.select_node( new_root );

    // .root
    {
        BOOST_TEST_REQUIRE( nw.selected_node() == new_root );

        BOOST_TEST( kmap.travel_left() );
        BOOST_TEST( nw.selected_node() == kmap.root_node_id() );
        BOOST_TEST( kmap.travel_left() );
        BOOST_TEST( kmap.travel_left() );
        BOOST_TEST( nw.selected_node() == kmap.root_node_id() );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == kmap.root_node_id() );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == kmap.root_node_id() );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == kmap.root_node_id() );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == kmap.root_node_id() );
    }
    // .root.<mid>
    {
        kmap.select_node( new_root );

        BOOST_TEST_REQUIRE( nw.selected_node() == new_root );

        auto const cids = nw.children( nw.selected_node() );

        BOOST_TEST_REQUIRE( cids.size() == 3 );

        auto const pid = mid( cids );
        auto const [ sids, offset ] = nw.siblings_inclusive( pid );

        BOOST_TEST_REQUIRE( cids.size() == sids.size() );

        BOOST_TEST( kmap.travel_right() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == sids[ offset - 1 ] );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == sids[ offset + 1 ] );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == pid );

        BOOST_TEST( kmap.travel_left() );
    }
    // .root.<top>
    {
        BOOST_TEST_REQUIRE( nw.selected_node() == new_root );

        auto const cids = nw.children( nw.selected_node() );

        BOOST_TEST_REQUIRE( cids.size() == 3 );

        auto const pid = cids[ 0 ];
        auto const [ sids, offset ] = nw.siblings_inclusive( pid );

        BOOST_TEST_REQUIRE( cids.size() == sids.size() );

        BOOST_TEST( kmap.travel_right() );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == sids[ offset + 1 ] );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == sids[ offset + 2 ] );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == sids[ offset + 1 ] );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_left() );
    }
    // .root.<bottom>
    {
        BOOST_TEST_REQUIRE( nw.selected_node() == new_root );

        auto const cids = nw.children( nw.selected_node() );

        BOOST_TEST_REQUIRE( cids.size() == 3 );

        auto const pid = cids[ 2 ];
        auto const [ sids, offset ] = nw.siblings_inclusive( pid );

        BOOST_TEST_REQUIRE( cids.size() == sids.size() );

        BOOST_TEST( kmap.travel_right() );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == sids[ offset - 1 ] );
        BOOST_TEST( kmap.travel_up() );
        BOOST_TEST( nw.selected_node() == sids[ offset - 2 ] );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == sids[ offset - 1 ] );
        BOOST_TEST( kmap.travel_down() );
        BOOST_TEST( nw.selected_node() == pid );
        BOOST_TEST( kmap.travel_left() );
    }

    BOOST_TEST( nw.selected_node() == new_root );
}
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test