/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "attribute.hpp"
#include "com/canvas/canvas.hpp"
#include "com/network/network.hpp"
#include "filesystem.hpp"
#include "js_iface.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

auto ordered_children( Kmap const& km
                     , Uuid const& parent )
{
    return view::make( parent )
         | view::child
         | view::to_node_set( km )
         | act::order( km );
}

SCENARIO( "kmap iface manipulation", "[kmap_iface]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );

    GIVEN( "blank starting state" )
    {
        WHEN( "root node created" )
        {
            auto const root = kmap.root_node_id();

            THEN( "root exists" )
            {
                REQUIRE( nw->exists( root ) );
            }
            THEN( "root is childless" )
            {
                REQUIRE( nw->fetch_children( root ).empty() );
            }
        }
    }
    GIVEN( "root node" )
    {
        auto const root = kmap.root_node_id();

        WHEN( "create child" )
        {
            auto const cv = REQUIRE_TRY( nw->create_child( kmap.root_node_id(), "h1" ) );

            REQUIRE( nw->fetch_children( root ).size() == 1 );

            THEN( "erase child" )
            {
                REQUIRE( succ( nw->erase_node( cv ) ) );
                REQUIRE( fail( nw->exists( cv ) ) );
                REQUIRE( fail( nw->fetch_heading( cv ) ) );
                REQUIRE( fail( nw->fetch_title( cv ) ) );
                {
                    #if KMAP_EXCEPTIONS_ENABLED
                    KMAP_DISABLE_EXCEPTION_LOG_SCOPED();
                    REQUIRE_THROWS( nw->fetch_children( cv ) ); // Currently, throws if cv doesn't exist. May change to Result< vector > in future.
                    #endif // KMAP_EXCEPTIONS_ENABLED
                }
                REQUIRE( nw->fetch_children( root ).empty() );
            }
        }
    }
    GIVEN( "two children of root" )
    {
        auto const root = kmap.root_node_id();
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "h1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "h2" ) );

        REQUIRE( nw->fetch_children( root ).size() == 2 );

        WHEN( "child 1 aliases child 2" )
        {
            REQUIRE( !nw->alias_store().is_alias( c2, c1 ) );
            REQUIRE( succ( nw->create_alias( c2, c1 ) ) );

            THEN( "alias is a child of 1" )
            {
                REQUIRE( nw->fetch_children( c1 ).size() == 1 );

                auto const ac = REQUIRE_TRY( view::make( c1 ) | view::child( "h2" ) | view::fetch_node( kmap ) );

                REQUIRE( nw->alias_store().is_alias( ac ) );
                REQUIRE( nw->alias_store().is_alias( c2, c1 ) );
                REQUIRE( nw->is_top_alias( ac ) );
                REQUIRE( nw->erase_node( ac ) );
            }
        }

        // TODO: I might know what's happening here. I could use better error reporting for sure, but the gist is that
        //       `erase_node()` is also erasing the parent-child relationship from the DB - but what if such a relationship is already marked as
        //       erased? Wouldn't that constitute an error? Need to confirm that we're duplicate erasing the same entry, to figure out if this the cause.
        //       Actually, I'm thinking this doesn't make sense... c1 and c2 are uniquely generated, so they should never overlap...
        REQUIRE( succ( nw->erase_node( c2 ) ) );
        REQUIRE( succ( nw->erase_node( c1 ) ) );
    }
    GIVEN( "alias exists" )
    {
        auto const root = kmap.root_node_id();
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "h1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "h2" ) );
        auto const ac = REQUIRE_TRY( nw->create_alias( c2, c1 ) );

        WHEN( "alias deleted" )
        {
            REQUIRE( succ( nw->erase_node( ac ) ) );

            THEN( "alias is no longer child of parent" )
            {
                REQUIRE( nw->fetch_children( c1 ).empty() );
            }
        }

        REQUIRE( succ( nw->erase_node( c2 ) ) );
        REQUIRE( succ( nw->erase_node( c1 ) ) );
    }
}

SCENARIO( "resolve alias", "[kmap_iface]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const root = kmap.root_node_id();

    GIVEN( "unnested alias" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const a21 = REQUIRE_TRY( nw->create_alias( c1, c2 ) );

        REQUIRE( !nw->alias_store().is_alias( c1 ) );
        REQUIRE( !nw->alias_store().is_alias( c2 ) );
        REQUIRE( nw->alias_store().is_alias( a21 ) );

        THEN( "resolves to source" )
        {
            REQUIRE( nw->resolve( a21 ) == c1 );
        }
    }
    GIVEN( "nested alias" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c11 = REQUIRE_TRY( nw->create_child( c1, "1" ) );
        auto const c111 = REQUIRE_TRY( nw->create_child( c11, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const a21 = REQUIRE_TRY( nw->create_alias( c1, c2 ) );
        auto const a211 = REQUIRE_TRY( nw->fetch_child( a21, "1" ) );
        auto const a2111 = REQUIRE_TRY( nw->fetch_child( a211, "1" ) );

        REQUIRE( !nw->alias_store().is_alias( c1 ) );
        REQUIRE( !nw->alias_store().is_alias( c11 ) );
        REQUIRE( !nw->alias_store().is_alias( c111 ) );
        REQUIRE( !nw->alias_store().is_alias( c2 ) );
        REQUIRE( nw->alias_store().is_alias( a21 ) );
        REQUIRE( nw->alias_store().is_alias( a211 ) );
        REQUIRE( nw->alias_store().is_alias( a2111 ) );

        THEN( "resolves to source" )
        {
            REQUIRE( nw->resolve( a21 ) == c1 );
            REQUIRE( nw->resolve( a211 ) == c11 );
            REQUIRE( nw->resolve( a2111 ) == c111 );
        }
    }
}

SCENARIO( "child ordering", "[kmap_iface]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const root = kmap.root_node_id();

    GIVEN( "two siblings" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "c1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "c2" ) );

        THEN( "siblings are ordered" )
        {
            REQUIRE_NOTHROW( ordered_children( kmap, root ) );
            auto const ordered = ordered_children( kmap, root );

            REQUIRE( ordered == std::vector{ c1, c2 } );
            REQUIRE( attr::is_in_order( kmap, root, c1 ) );
            REQUIRE( attr::is_in_order( kmap, root, c2 ) );
        }

        WHEN( "one child is erased" )
        {
            REQUIRE( succ( nw->erase_node( c1 ) ) );

            THEN( "ordering is empty and well formed" )
            {
                REQUIRE( !attr::is_in_order( kmap, root, c1 ) );
                REQUIRE( attr::is_in_order( kmap, root, c2 ) );

                auto const ordern = REQUIRE_TRY( view::make( root )
                                               | view::attr
                                               | view::child( "order" )
                                               | view::fetch_node( kmap ) );
                auto const orderb = REQUIRE_TRY( nw->fetch_body( ordern ) );

                REQUIRE( orderb == to_string( c2 ) );
            }
        }
        WHEN( "siblings/all children are erased" )
        {
            REQUIRE( succ( nw->erase_node( c1 ) ) );
            REQUIRE( succ( nw->erase_node( c2 ) ) );
            
            THEN( "parent ordering is erased" )
            {
                REQUIRE( !attr::is_in_order( kmap, root, c1 ) );
                REQUIRE( !attr::is_in_order( kmap, root, c2 ) );
                REQUIRE( !( view::make( root )
                          | view::attr
                          | view::child( "order" )
                          | view::exists( kmap ) ) );
            }

            WHEN( "child is created" )
            {
                auto const c3 = REQUIRE_TRY( nw->create_child( root, "c3" ) );

                THEN( "child exists" )
                {
                    REQUIRE( nw->exists( c3 ) );
                }
                THEN( "child is ordered" )
                {
                    REQUIRE_NOTHROW( ordered_children( kmap, root ) );
                    auto const ordered = ordered_children( kmap, root );

                    REQUIRE( ordered == std::vector{ c3 } );
                }
            }
        }
    }
}

SCENARIO( "alias creates attr::order", "[kmap_iface]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const root = kmap.root_node_id();

    GIVEN( "two children" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

        WHEN( "create one alias" )
        {
            auto const a1 = REQUIRE_TRY( nw->create_alias( c2, c1 ) );

            THEN( "resolved alias is in order of parent" )
            {
                REQUIRE( attr::is_in_order( kmap, c1, nw->resolve( a1 ) ) );
            }

            REQUIRE( succ( nw->erase_node( a1 ) ) );
            REQUIRE( !attr::is_in_order( kmap, c1, nw->resolve( a1 ) ) );
        }

        REQUIRE( succ( nw->erase_node( c2 ) ) );
        REQUIRE( succ( nw->erase_node( c1 ) ) );
    }
    GIVEN( "three children" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const c3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

        WHEN( "create two aliases" )
        {
            auto const a1 = REQUIRE_TRY( nw->create_alias( c2, c1 ) );
            auto const a2 = REQUIRE_TRY( nw->create_alias( c3, c1 ) );

            THEN( "resolved alias is in order of parent" )
            {
                REQUIRE( attr::is_in_order( kmap, c1, nw->resolve( a1 ) ) );
                REQUIRE( attr::is_in_order( kmap, c1, nw->resolve( a2 ) ) );
            }

            REQUIRE( succ( nw->erase_node( a1 ) ) );
            REQUIRE( !attr::is_in_order( kmap, c1, nw->resolve( a1 ) ) );
            REQUIRE( succ( nw->erase_node( a2 ) ) );
            REQUIRE( !attr::is_in_order( kmap, c1, nw->resolve( a2 ) ) );
        }

        REQUIRE( succ( nw->erase_node( c3 ) ) );
        REQUIRE( succ( nw->erase_node( c2 ) ) );
        REQUIRE( succ( nw->erase_node( c1 ) ) );
    }
}

SCENARIO( "fetch ordered children of alias", "[kmap_iface]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const root = kmap.root_node_id();

    GIVEN( "alias with no child" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const a1 = REQUIRE_TRY( nw->create_alias( c2, c1 ) );

        WHEN( "ordered children fetched" )
        {
            REQUIRE_NOTHROW( ordered_children( kmap, a1 ) );

            THEN( "alias has no children" )
            {
                REQUIRE( ordered_children( kmap, a1 ).empty() );
            }
        }

        REQUIRE( nw->erase_node( a1 ) );
        REQUIRE( nw->erase_node( c2 ) );
        REQUIRE( nw->erase_node( c1 ) );
    }
    GIVEN( "alias with child" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const c21 = REQUIRE_TRY( nw->create_child( c2, "1" ) );
        auto const a1 = REQUIRE_TRY( nw->create_alias( c2, c1 ) );

        WHEN( "ordered children fetched" )
        {
            REQUIRE_NOTHROW( ordered_children( kmap, a1 ) );

            THEN( "alias has no children" )
            {
                REQUIRE( !ordered_children( kmap, a1 ).empty() );
                REQUIRE( nw->resolve( ordered_children( kmap, a1 ).at( 0 ) ) == c21 );
            }
        }

        REQUIRE( nw->erase_node( a1 ) );
        REQUIRE( nw->erase_node( c2 ) );
        REQUIRE( nw->erase_node( c1 ) );
    }
}

// TODO: Belongs in alias.cpp
SCENARIO( "erase nested alias source", "[kmap_iface]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& kmap = Singleton::instance();
    auto const root = kmap.root_node_id();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );

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
                    | view::single
                    | view::exists( kmap ) ));
            REQUIRE(( view::make( root ) 
                    | view::child( "alias_root" )
                    | view::direct_desc( "1.2.3" )
                    | view::single
                    | view::exists( kmap ) ));
            REQUIRE(( view::make( root ) 
                    | view::child( "alias_root" )
                    | view::alias // 1
                    | view::alias // 2
                    | view::alias // 3
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

            THEN( "alias dst erased, too" )
            {
                REQUIRE( nw->fetch_children( anode ).empty() );
                REQUIRE( ordered_children( kmap, anode ).empty() );
                REQUIRE( fail( view::make( root ) | view::direct_desc( "alias_root.1.2.3" ) | view::fetch_node( kmap ) ) );
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
                REQUIRE( nw->fetch_children( anode ).empty() );
                REQUIRE( ordered_children( kmap, anode ).empty() );
                REQUIRE( fail( view::make( root ) | view::direct_desc( "alias_root.1.2" ) | view::fetch_node( kmap ) ) );
                REQUIRE( fail( view::make( root ) | view::direct_desc( "alias_root.1.2.3" ) | view::fetch_node( kmap ) ) );
            }
            THEN( "erased alias dst parent exists" )
            {
                REQUIRE_RES( view::make( root ) | view::direct_desc( "alias_root.1" ) | view::fetch_node( kmap ) );
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
                REQUIRE( nw->fetch_children( aroot ).empty() );
                REQUIRE( ordered_children( kmap, aroot ).empty() );
            }
        }
    }
}
