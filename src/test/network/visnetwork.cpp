/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"
#include "com/canvas/canvas.hpp"
#include "com/visnetwork/visnetwork.hpp"
#include "js_iface.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "visual network testing", "[visnw][env]" )
{
    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::VisualNetwork >() );

    GIVEN( "blank network" )
    {
        auto const root_id = kmap.root_node_id();
        auto const root_title = "Root";

        WHEN( "create initial node" )
        {
            REQUIRE( succ( nw->create_node( root_id, root_title ) ) );

            THEN( "node exists" )
            {
                REQUIRE( nw->exists( root_id ) );
            }
            THEN( "title matches" )
            {
                REQUIRE( succ( nw->fetch_title( root_id ) ) );
                REQUIRE( nw->fetch_title( root_id ).value() == root_title );
            }
            THEN( "childless" )
            {
                REQUIRE( nw->children( root_id ).empty() );
            }
            THEN( "parentless" )
            {
                REQUIRE( fail( nw->fetch_parent( root_id ) ) );
            }

            REQUIRE_RES( nw->erase_node( root_id ) );
        }

    }
    GIVEN( "root node" )
    {
        auto const root_id = gen_uuid();
        auto const root_title = "Root";

        REQUIRE( succ( nw->create_node( root_id, root_title ) ) );

        WHEN( "create child" )
        {
            auto const child_id = gen_uuid();
            auto const child_title = "Child";

            REQUIRE( succ( nw->create_node( child_id, child_title ) ) );
            REQUIRE( fail( nw->edge_exists( root_id, child_id ) ) );
            REQUIRE( succ( nw->add_edge( root_id, child_id ) ) );

            THEN( "child exists" )
            {
                REQUIRE( nw->children( root_id ).size() == 1 );
                REQUIRE( nw->children( root_id ).at( 0 ) == child_id );
                REQUIRE( nw->fetch_title( child_id ).value() == child_title );
            }
            THEN( "edge between root and child" )
            {
                REQUIRE( nw->edge_exists( root_id, child_id ) );
            }

            REQUIRE( succ( nw->remove_edge( root_id, child_id ) ) );
            REQUIRE( succ( nw->erase_node( child_id ) ) );
            REQUIRE( succ( nw->erase_node( root_id ) ) );
        }

        REQUIRE( succ( nw->create_node( root_id, root_title ) ) );
        REQUIRE( succ( nw->erase_node( root_id ) ) );
    }
}
