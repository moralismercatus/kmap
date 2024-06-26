/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/order.hpp>

#include <common.hpp>
#include <path/view/act/fetch_node.hpp>
#include <path/view/act/to_node_set.hpp>
#include <path/view/anchor/node.hpp>
#include <path/view/child.hpp>
#include <path/view/right_lineal.hpp>
#include <test/util.hpp>

#include <catch2/catch_test_macros.hpp>

namespace kmap::path::detail {

auto order( Kmap const& km
          , UuidVec& ordered_nodes
          , Uuid const& root
          , std::vector< UuidVec > const& lineages )
    -> Result< void >
{
    namespace rvs = ranges::views;

    KM_RESULT_PROLOG();

    // TODO: precondition... all lineages are nonempty.

    if( lineages.size() == 0 ) // Done; nothing to order.
    {
        return outcome::success();
    }
    if( lineages.size() == 1 ) // Done; nothing to order.
    {
        BC_ASSERT( !lineages.at( 0 ).empty() );
        ordered_nodes.emplace_back( lineages.at( 0 ).back() );

        return outcome::success();
    }

    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto const ordered_siblings = KTRY( nw->fetch_children_ordered( root ) );
    auto mlineages = lineages;

    // Iterate via ordered siblings, to ensure order of lineage processing.
    for( auto const& sibling : ordered_siblings )
    {
        // Short-circuit if nothing more to order.
        if( mlineages.empty() )
        {
            break;
        }

        // Gather lineages with sibling root.
        auto matching_lineages = decltype( mlineages ){};

        for( auto it = mlineages.begin()
           ; it != mlineages.end()
           ; )
        {
            if( sibling == it->at( 0 ) )
            {
                matching_lineages.emplace_back( *it );

                it = mlineages.erase( it );
            }
            else
            {
                ++it;
            }
        }

        // Process lineages with sibling root.
        if( !matching_lineages.empty() )
        {
            auto const common_root = matching_lineages.at( 0 ).front();
            auto const sub_lineages = [ & ]
            {
                auto t = decltype( matching_lineages ){};
                auto const common_root = matching_lineages.at( 0 ).front();
                for( auto const& lin : matching_lineages )
                {
                    BC_ASSERT( lin.at( 0 ) == common_root );
                    if( lin.size() == 1 )
                    {
                        ordered_nodes.emplace_back( lin.at( 0 ) );
                    }
                    else
                    {
                        t.emplace_back( lin | rvs::drop( 1 ) | ranges::to< UuidVec >() );
                    }
                }
                return t;
            }();

            KTRY( order( km, ordered_nodes, common_root, sub_lineages ) );
        }
    }

    return outcome::success();
}

} // namespace kmap::path::detail

namespace kmap::path {

SCENARIO( "path::order", "[order][path]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/" )
    {
        THEN( "order( {} ) => {}" )
        {
            auto const ov = REQUIRE_TRY( order( km, UuidVec{} ) );

            REQUIRE( ov == UuidVec{} );
        }
        THEN( "order( / ) => /" )
        {
            auto const ov = REQUIRE_TRY( order( km, UuidVec{ root } ) );

            REQUIRE( ov == UuidVec{ root } );
        }
        THEN( "order( /, / ) => fail" )
        {
            REQUIRE_RFAIL( order( km, UuidVec{ root, root } ) );
        }

        GIVEN( "/1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "order => { /, 1 }" )
            {
                auto const ov = REQUIRE_TRY( order( km, UuidVec{ n1, root } ) );

                REQUIRE( ov == UuidVec{ root, n1 } );
            }

            GIVEN( "/2" )
            {
                auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                THEN( "order => { /, 1, 2 }" )
                {
                    auto const uov = anchor::node( root ) | view2::right_lineal | act2::to_node_set( km );
                    auto const ov = REQUIRE_TRY( order( km, uov ) );

                    REQUIRE( ov == UuidVec{ root, n1, n2 } );
                }

                GIVEN( "/3" )
                {
                    auto const n3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

                    THEN( "order => { /, 1, 2, 3 }" )
                    {
                        auto const uov = anchor::node( root ) | view2::right_lineal | act2::to_node_set( km );
                        auto const ov = REQUIRE_TRY( order( km, uov ) );

                        REQUIRE( ov == UuidVec{ root, n1, n2, n3 } );
                    }

                    GIVEN( "alias /2.1[/1]" )
                    {
                        auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

                        THEN( "order => { /, 1, 2, 2.1[/1], 3 }" )
                        {
                            auto const uov = anchor::node( root ) | view2::right_lineal | act2::to_node_set( km );
                            auto const ov = REQUIRE_TRY( order( km, uov ) );

                            REQUIRE( ov == UuidVec{ root
                                                  , n1
                                                  , n2, a21
                                                  , n3 } );
                        }

                        GIVEN( "alias /3.2[/2]" )
                        {
                            auto const a32 = REQUIRE_TRY( nw->create_alias( n2, n3 ) );
                            auto const a321 = REQUIRE_TRY( anchor::node( a32 ) | view2::alias | act2::fetch_node( km ) );

                            THEN( "order => { /, 1, 2, 2.1[/1], 3, 3.2[/2], 3.2.1[/1] }" )
                            {
                                auto const uov = anchor::node( root ) | view2::right_lineal | act2::to_node_set( km );
                                auto const ov = REQUIRE_TRY( order( km, uov ) );

                                REQUIRE( ov == UuidVec{ root
                                                      , n1
                                                      , n2, a21
                                                      , n3, a32, a321 } );
                            }

                            GIVEN( "/1.x" )
                            {
                                auto const n1x = REQUIRE_TRY( nw->create_child( n1, "x" ) );
                                auto const a21x = REQUIRE_TRY( anchor::node( a21 ) | view2::alias( view2::resolve( n1x ) ) | act2::fetch_node( km ) );
                                auto const a321x = REQUIRE_TRY( anchor::node( a321 ) | view2::alias( view2::resolve( n1x ) ) | act2::fetch_node( km ) );

                                THEN( "order => { /, 1, 1.x, 2, 2.1[/1], 2.1.x[/1.x], 3, 3.2[/2], 3.2.1[/1], 3.2.1.x[/1.x] }" )
                                {
                                    auto const uov = anchor::node( root ) | view2::right_lineal | act2::to_node_set( km );
                                    auto const ov = REQUIRE_TRY( order( km, uov ) );

                                    REQUIRE( ov == UuidVec{ root
                                                          , n1, n1x
                                                          , n2, a21, a21x
                                                          , n3, a32, a321, a321x } );
                                }

                                GIVEN( "/1.y" )
                                {
                                    auto const n1y = REQUIRE_TRY( nw->create_child( n1, "y" ) );
                                    auto const a21y = REQUIRE_TRY( anchor::node( a21 ) | view2::alias( view2::resolve( n1y ) ) | act2::fetch_node( km ) );
                                    auto const a321y = REQUIRE_TRY( anchor::node( a321 ) | view2::alias( view2::resolve( n1y ) ) | act2::fetch_node( km ) );

                                    THEN( "order => { /, 1, 1.x, 1.y, 2, 2.1[/1], 2.1.x[/1.x], 2.1.y[/1.y], 3, 3.2[/2], 3.2.1[/1], 3.2.1.x[/1.x], 3.2.1.y[/1.y] }" )
                                    {
                                        auto const uov = anchor::node( root ) | view2::right_lineal | act2::to_node_set( km );
                                        auto const ov = REQUIRE_TRY( order( km, uov ) );

                                        REQUIRE( ov == UuidVec{ root
                                                              , n1, n1x, n1y
                                                              , n2, a21, a21x, a21y
                                                              , n3, a32, a321, a321x, a321y } );
                                    }
                                    THEN( "3.2.1 | child | order => { 3.2.1.x[1.x], 3.2.1.y[1.y] }" )
                                    {
                                        auto const uov = anchor::node( a321 ) | view2::child | act2::to_node_set( km );
                                        auto const ov = REQUIRE_TRY( order( km, uov ) );

                                        REQUIRE( ov == UuidVec{ a321x, a321y } );
                                    }

                                    GIVEN( "/1.z" )
                                    {
                                        auto const n1z = REQUIRE_TRY( nw->create_child( n1, "z" ) );
                                        auto const a21z = REQUIRE_TRY( anchor::node( a21 ) | view2::alias( view2::resolve( n1z ) ) | act2::fetch_node( km ) );
                                        auto const a321z = REQUIRE_TRY( anchor::node( a321 ) | view2::alias( view2::resolve( n1z ) ) | act2::fetch_node( km ) );

                                        THEN( "order => { /, 1, 1.x, 1.y, 2, 2.1[/1], 2.1.x[/1.x], 2.1.y[/1.y], 3, 3.2[/2], 3.2.1[/1], 3.2.1.x[/1.x], 3.2.1.y[/1.y] }" )
                                        {
                                            auto const uov = anchor::node( root ) | view2::right_lineal | act2::to_node_set( km );
                                            auto const ov = REQUIRE_TRY( order( km, uov ) );

                                            REQUIRE( ov == UuidVec{ root
                                                                , n1, n1x, n1y, n1z
                                                                , n2, a21, a21x, a21y, a21z
                                                                , n3, a32, a321, a321x, a321y, a321z } );
                                        }
                                        THEN( "3.2.1 | child | order => { 3.2.1.x[1.x], 3.2.1.y[1.y] }" )
                                        {
                                            auto const uov = anchor::node( a321 ) | view2::child | act2::to_node_set( km );
                                            auto const ov = REQUIRE_TRY( order( km, uov ) );

                                            REQUIRE( ov == UuidVec{ a321x, a321y, a321z } );
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace kmap::view::act