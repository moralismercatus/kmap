/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "attribute.hpp"
#include "canvas.hpp"
#include "filesystem.hpp"
#include "js_iface.hpp"
#include "test/util.hpp"

#include <boost/test/unit_test.hpp>
#include <catch2/catch_test_macros.hpp>

namespace utf = boost::unit_test;
using namespace kmap;
using namespace kmap::test;

SCENARIO( "kmap iface manipulation", "[kmap_iface]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();

    GIVEN( "blank starting state" )
    {
        WHEN( "root node created" )
        {
            auto const root = kmap.root_node_id();

            THEN( "root exists" )
            {
                REQUIRE( kmap.exists( root ) );
            }
            THEN( "root is childless" )
            {
                REQUIRE( kmap.fetch_children( root ).empty() );
            }
        }
    }
    GIVEN( "root node" )
    {
        auto const root = kmap.root_node_id();

        WHEN( "create child" )
        {
            auto const child = kmap.create_child( kmap.root_node_id(), "h1" );

            REQUIRE( succ( child ) );
            REQUIRE( kmap.fetch_children( root ).size() == 1 );

            auto const cv = child.value();

            THEN( "erase child" )
            {
                REQUIRE( succ( kmap.erase_node( cv ) ) );
                REQUIRE( fail( kmap.exists( cv ) ) );
                REQUIRE( fail( kmap.fetch_heading( cv ) ) );
                REQUIRE( fail( kmap.fetch_title( cv ) ) );
                {
                    KMAP_DISABLE_EXCEPTION_LOG_SCOPED();
                    REQUIRE_THROWS( kmap.fetch_children( cv ) ); // Currently, throws if cv doesn't exist. May change to Result< vector > in future.
                }
                REQUIRE( kmap.fetch_children( root ).empty() );
            }
        }
    }
    GIVEN( "two children of root" )
    {
        auto const root = kmap.root_node_id();
        auto const c1 = kmap.create_child( root, "h1" );
        auto const c2 = kmap.create_child( root, "h2" );

        REQUIRE( kmap.fetch_children( root ).size() == 2 );

        REQUIRE( succ( c1 ) );
        REQUIRE( succ( c2 ) );

        auto const c1v = c1.value();
        auto const c2v = c2.value();

        WHEN( "child 1 aliases child 2" )
        {
            REQUIRE( !kmap.is_alias( c2v, c1v ) );
            REQUIRE( succ( kmap.create_alias( c2v, c1v ) ) );

            THEN( "alias is a child of 1" )
            {
                REQUIRE( kmap.fetch_children( c1v ).size() == 1 );

                auto const ac = kmap.fetch_child( c1v, "h2" );

                REQUIRE( succ( ac ) );
                REQUIRE( kmap.is_alias( ac.value() ) );
                REQUIRE( kmap.is_alias( c2v, c1v ) );
                REQUIRE( kmap.is_top_alias( ac.value() ) );
                REQUIRE( kmap.erase_node( ac.value() ) );
            }
        }

        // TODO: I might know what's happening here. I could use better error reporting for sure, but the gist is that
        //       `erase_node()` is also erasing the parent-child relationship from the DB - but what if such a relationship is already marked as
        //       erased? Wouldn't that constitute an error? Need to confirm that we're duplicate erasing the same entry, to figure out if this the cause.
        //       Actually, I'm thinking this doesn't make sense... c1 and c2 are uniquely generated, so they should never overlap...
        REQUIRE( succ( kmap.erase_node( c2v ) ) );
        REQUIRE( succ( kmap.erase_node( c1v ) ) );
    }
    GIVEN( "alias exists" )
    {
        auto const root = kmap.root_node_id();
        auto const c1 = kmap.create_child( root, "h1" ).value();
        auto const c2 = kmap.create_child( root, "h2" ).value();
        auto const ac = kmap.create_alias( c2, c1 ).value();

        WHEN( "alias deleted" )
        {
            REQUIRE( succ( kmap.erase_node( ac ) ) );

            THEN( "alias is no longer child of parent" )
            {
                REQUIRE( kmap.fetch_children( c1 ).empty() );
            }
        }

        REQUIRE( succ( kmap.erase_node( c2 ) ) );
        REQUIRE( succ( kmap.erase_node( c1 ) ) );
    }
}

SCENARIO( "resolve alias", "[kmap_iface]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "unnested alias" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "2" ) );
        auto const a21 = REQUIRE_TRY( kmap.create_alias( c1, c2 ) );

        REQUIRE( !kmap.is_alias( c1 ) );
        REQUIRE( !kmap.is_alias( c2 ) );
        REQUIRE( kmap.is_alias( a21 ) );

        THEN( "resolves to source" )
        {
            REQUIRE( kmap.resolve( a21 ) == c1 );
        }
    }
    GIVEN( "nested alias" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "1" ) );
        auto const c11 = REQUIRE_TRY( kmap.create_child( c1, "1" ) );
        auto const c111 = REQUIRE_TRY( kmap.create_child( c11, "1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "2" ) );
        auto const a21 = REQUIRE_TRY( kmap.create_alias( c1, c2 ) );
        auto const a211 = REQUIRE_TRY( kmap.fetch_child( a21, "1" ) );
        auto const a2111 = REQUIRE_TRY( kmap.fetch_child( a211, "1" ) );

        REQUIRE( !kmap.is_alias( c1 ) );
        REQUIRE( !kmap.is_alias( c11 ) );
        REQUIRE( !kmap.is_alias( c111 ) );
        REQUIRE( !kmap.is_alias( c2 ) );
        REQUIRE( kmap.is_alias( a21 ) );
        REQUIRE( kmap.is_alias( a211 ) );
        REQUIRE( kmap.is_alias( a2111 ) );

        THEN( "resolves to source" )
        {
            REQUIRE( kmap.resolve( a21 ) == c1 );
            REQUIRE( kmap.resolve( a211 ) == c11 );
            REQUIRE( kmap.resolve( a2111 ) == c111 );
        }
    }
}

SCENARIO( "child ordering", "[kmap_iface]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "two siblings" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "c1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "c2" ) );

        THEN( "siblings are ordered" )
        {
            REQUIRE_NOTHROW( kmap.fetch_children_ordered( root ) );
            auto const ordered = kmap.fetch_children_ordered( root );

            REQUIRE( ordered == std::vector{ c1, c2 } );
            REQUIRE( attr::is_in_order( kmap, root, c1 ) );
            REQUIRE( attr::is_in_order( kmap, root, c2 ) );
        }

        WHEN( "siblings erased" )
        {
            REQUIRE( succ( kmap.erase_node( c1 ) ) );
            REQUIRE( succ( kmap.erase_node( c2 ) ) );
            
            THEN( "ordering is empty and well formed" )
            {
                REQUIRE( !attr::is_in_order( kmap, root, c1 ) );
                REQUIRE( !attr::is_in_order( kmap, root, c2 ) );

                auto const ordern = REQUIRE_TRY( view::make( root )
                                               | view::attr
                                               | view::child( "order" )
                                               | view::fetch_node( kmap ) );
                auto const orderb = REQUIRE_TRY( kmap.fetch_body( ordern ) );

                REQUIRE( orderb.empty() );
            }

            WHEN( "child is created" )
            {
                auto const c3 = REQUIRE_TRY( kmap.create_child( root, "c3" ) );

                THEN( "child exists" )
                {
                    REQUIRE( kmap.exists( c3 ) );
                }
                THEN( "child is ordered" )
                {
                    REQUIRE_NOTHROW( kmap.fetch_children_ordered( root ) );
                    auto const ordered = kmap.fetch_children_ordered( root );

                    REQUIRE( ordered == std::vector{ c3 } );
                }
            }
        }
    }
}

SCENARIO( "alias creates attr::order", "[kmap_iface]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "two children" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "2" ) );

        WHEN( "create one alias" )
        {
            auto const a1 = REQUIRE_TRY( kmap.create_alias( c2, c1 ) );

            THEN( "resolved alias is in order of parent" )
            {
                REQUIRE( attr::is_in_order( kmap, c1, kmap.resolve( a1 ) ) );
            }

            REQUIRE( succ( kmap.erase_node( a1 ) ) );
            REQUIRE( !attr::is_in_order( kmap, c1, kmap.resolve( a1 ) ) );
        }

        REQUIRE( succ( kmap.erase_node( c2 ) ) );
        REQUIRE( succ( kmap.erase_node( c1 ) ) );
    }
    GIVEN( "three children" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "2" ) );
        auto const c3 = REQUIRE_TRY( kmap.create_child( root, "3" ) );

        WHEN( "create two aliases" )
        {
            auto const a1 = REQUIRE_TRY( kmap.create_alias( c2, c1 ) );
            auto const a2 = REQUIRE_TRY( kmap.create_alias( c3, c1 ) );

            THEN( "resolved alias is in order of parent" )
            {
                REQUIRE( attr::is_in_order( kmap, c1, kmap.resolve( a1 ) ) );
                REQUIRE( attr::is_in_order( kmap, c1, kmap.resolve( a2 ) ) );
            }

            REQUIRE( succ( kmap.erase_node( a1 ) ) );
            REQUIRE( !attr::is_in_order( kmap, c1, kmap.resolve( a1 ) ) );
            REQUIRE( succ( kmap.erase_node( a2 ) ) );
            REQUIRE( !attr::is_in_order( kmap, c1, kmap.resolve( a2 ) ) );
        }

        REQUIRE( succ( kmap.erase_node( c3 ) ) );
        REQUIRE( succ( kmap.erase_node( c2 ) ) );
        REQUIRE( succ( kmap.erase_node( c1 ) ) );
    }
}

SCENARIO( "fetch ordered children of alias", "[kmap_iface]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "alias with no child" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "2" ) );
        auto const a1 = REQUIRE_TRY( kmap.create_alias( c2, c1 ) );

        WHEN( "ordered children fetched" )
        {
            REQUIRE_NOTHROW( kmap.fetch_children_ordered( a1 ) );

            THEN( "alias has no children" )
            {
                REQUIRE( kmap.fetch_children_ordered( a1 ).empty() );
            }
        }

        REQUIRE( kmap.erase_node( a1 ) );
        REQUIRE( kmap.erase_node( c2 ) );
        REQUIRE( kmap.erase_node( c1 ) );
    }
    GIVEN( "alias with child" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "2" ) );
        auto const c21 = REQUIRE_TRY( kmap.create_child( c2, "1" ) );
        auto const a1 = REQUIRE_TRY( kmap.create_alias( c2, c1 ) );

        WHEN( "ordered children fetched" )
        {
            REQUIRE_NOTHROW( kmap.fetch_children_ordered( a1 ) );

            THEN( "alias has no children" )
            {
                REQUIRE( !kmap.fetch_children_ordered( a1 ).empty() );
                REQUIRE( kmap.resolve( kmap.fetch_children_ordered( a1 ).at( 0 ) ) == c21 );
            }
        }

        REQUIRE( kmap.erase_node( a1 ) );
        REQUIRE( kmap.erase_node( c2 ) );
        REQUIRE( kmap.erase_node( c1 ) );
    }
}

SCENARIO( "erase nested alias source", "[kmap_iface]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "nested alias" )
    {
        REQUIRE_RES( view::make( root )
                   | view::direct_desc( "1.2.3" )
                   | view::create_node( kmap ) );
        REQUIRE_RES( view::make( root )
                   | view::child( "alias_root" )
                   | view::alias( view::make( root ) | view::child( "1" ) )
                   | view::create_node( kmap ) );

        THEN( "aliases exist" )
        {
            REQUIRE(( view::make( root ) 
                    | view::child( "alias_root" )
                    | view::alias
                    | view::alias
                    | view::alias
                    | view::single
                    | view::exists( kmap ) ));
        }

        WHEN( "erase alias leaf source" )
        {
            REQUIRE_RES( view::make( root )
                       | view::direct_desc( "1.2.3" )
                       | view::erase_node( kmap ) );
            auto const anode = REQUIRE_TRY( view::make( root ) 
                                          | view::child( "alias_root" )
                                          | view::direct_desc( "1.2" )
                                          | view::fetch_node( kmap ) );
            // KTRYE( print_tree( kmap, root ) );

            THEN( "alias dst erased, too" )
            {
                REQUIRE( kmap.fetch_children( anode ).empty() );
                REQUIRE( kmap.fetch_children_ordered( anode ).empty() );
                REQUIRE( fail( kmap.fetch_descendant( root, "alias_root.1.2.3" ) ) );
            }
        }
        WHEN( "erase alias middle source" )
        {
            REQUIRE_RES( view::make( root )
                       | view::direct_desc( "1.2" )
                       | view::erase_node( kmap ) );
            auto const anode = REQUIRE_TRY( view::make( root ) 
                                          | view::child( "alias_root" )
                                          | view::direct_desc( "1" )
                                          | view::fetch_node( kmap ) );

            THEN( "alias dst erased, too" )
            {
                REQUIRE( kmap.fetch_children( anode ).empty() );
                REQUIRE( kmap.fetch_children_ordered( anode ).empty() );
                REQUIRE( fail( kmap.fetch_descendant( root, "alias_root.1.2" ) ) );
                REQUIRE( fail( kmap.fetch_descendant( root, "alias_root.1.2.3" ) ) );
            }
            THEN( "erased alias dst parent exists" )
            {
                REQUIRE_RES( kmap.fetch_descendant( root, "alias_root.1" ) );
            }
        }
        WHEN( "erase alias root source" )
        {
            REQUIRE_RES( view::make( root )
                       | view::child( "1" )
                       | view::erase_node( kmap ) );
            auto const aroot = REQUIRE_TRY( view::make( root ) 
                                          | view::child( "alias_root" )
                                          | view::fetch_node( kmap ) );

            THEN( "alias dst erased, too" )
            {
                REQUIRE( kmap.fetch_children( aroot ).empty() );
                REQUIRE( kmap.fetch_children_ordered( aroot).empty() );
            }
        }
    }
}

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( kmap_iface 
                     , 
                     * utf::depends_on( "database" )
                     * utf::depends_on( "filesystem" )
                    //  * utf::depends_on( "network" )
                     * utf::depends_on( "path" ) )
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test