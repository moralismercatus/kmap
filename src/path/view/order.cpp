/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/order.hpp>

#include <contract.hpp>
#include <path/act/order.hpp>
#include <path/view/act/to_node_vec.hpp>
#include <test/util.hpp>

#include <catch2/catch_test_macros.hpp>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Order::fetch( FetchContext const& ctx
                 , FetchSet const& fs ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();

    auto rv = FetchSet{};
    auto const ns = fs
                  | rvs::transform( [ & ]( auto const& e ){ return e.id; } )
                  | ranges::to< std::vector< Uuid > >();

    auto const ordered = ns | view::act::order( ctx.km );

    for( auto const& n : ordered )
    {
        rv.insert( LinkNode{ .id = n } );
    }

    return rv;
}

SCENARIO( "view::order", "[order][node_view]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/" )
    {
        THEN( "order( / ) => /" )
        {
            auto const ov = anchor::node( root ) | view2::order | act2::to_node_vec( km );

            REQUIRE( ov == UuidVec{ root } );
        }

        GIVEN( "/1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "order => { /, 1 }" )
            {
                auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

                REQUIRE( ov == UuidVec{ root, n1 } );
            }

            GIVEN( "/2" )
            {
                auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                THEN( "order => { /, 1, 2 }" )
                {
                    auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

                    REQUIRE( ov == UuidVec{ root, n1, n2 } );
                }

                GIVEN( "/3" )
                {
                    auto const n3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

                    THEN( "order => { /, 1, 2, 3 }" )
                    {
                        auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

                        REQUIRE( ov == UuidVec{ root, n1, n2, n3 } );
                    }

                    GIVEN( "alias /2.1[/1]" )
                    {
                        auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

                        THEN( "order => { /, 1, 2, 2.1[/1], 3 }" )
                        {
                            auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

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
                                auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

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
                                    auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

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
                                        auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

                                        REQUIRE( ov == UuidVec{ root
                                                              , n1, n1x, n1y
                                                              , n2, a21, a21x, a21y
                                                              , n3, a32, a321, a321x, a321y } );
                                    }
                                    THEN( "3.2.1 | child | order => { 3.2.1.x[1.x], 3.2.1.y[1.y] }" )
                                    {
                                        auto const ov = anchor::node( a321 ) | view2::child | view2::order | act2::to_node_vec( km );

                                        REQUIRE( ov == UuidVec{ a321x, a321y } );
                                    }

                                    GIVEN( "/1.z" )
                                    {
                                        KMAP_LOG_LINE();
                                        auto const n1z = REQUIRE_TRY( nw->create_child( n1, "z" ) );
                                        KMAP_LOG_LINE();
                                        auto const a21z = REQUIRE_TRY( anchor::node( a21 ) | view2::alias( view2::resolve( n1z ) ) | act2::fetch_node( km ) );
                                        auto const a321z = REQUIRE_TRY( anchor::node( a321 ) | view2::alias( view2::resolve( n1z ) ) | act2::fetch_node( km ) );

                                        THEN( "order => { /, 1, 1.x, 1.y, 2, 2.1[/1], 2.1.x[/1.x], 2.1.y[/1.y], 3, 3.2[/2], 3.2.1[/1], 3.2.1.x[/1.x], 3.2.1.y[/1.y] }" )
                                        {
                                            auto const ov = anchor::node( root ) | view2::right_lineal | view2::order | act2::to_node_vec( km );

                                            REQUIRE( ov == UuidVec{ root
                                                                , n1, n1x, n1y, n1z
                                                                , n2, a21, a21x, a21y, a21z
                                                                , n3, a32, a321, a321x, a321y, a321z } );
                                        }
                                        THEN( "3.2.1 | child | order => { 3.2.1.x[1.x], 3.2.1.y[1.y] }" )
                                        {
                                            auto const ov = anchor::node( a321 ) | view2::child | view2::order | act2::to_node_vec( km );

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

auto Order::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Order >();
}

auto Order::to_string() const
    -> std::string
{
    return "order";
}

auto Order::compare_less( Link const& other ) const
    -> bool
{
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return false; //return pred_ < dynamic_cast< decltype( *this )& >( other ).pred_;
    }
    else if( cmp == std::strong_ordering::less )
    {
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace kmap::view2
