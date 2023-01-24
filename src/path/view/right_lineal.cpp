/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/right_lineal.hpp"

#include "com/network/network.hpp"
#include "path.hpp"
#include "path/view/anchor/root.hpp"
#include "path/view/desc.hpp"
#include "test/util.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

#include <compare>
#include <variant>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto RightLineal::create( CreateContext const& ctx
                       , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "this is not what you want" );
}

auto RightLineal::fetch( FetchContext const& ctx
                      , Uuid const& node ) const
    -> FetchSet
{
    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred )
            {
                return view2::right_lineal( std::string{ pred } ).fetch( ctx, node );
            }
        ,   [ & ]( std::string const& pred )
            {
                auto rs = FetchSet{};
                auto const right_lineals = view2::right_lineal.fetch( ctx, node );
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );

                return right_lineals
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );

                if( is_lineal( *nw, node, pred ) )
                {
                    return FetchSet{ LinkNode{ .id = pred } };
                }
                else
                {
                    return FetchSet{};
                }
            }
        ,   [ & ]( LinkPtr const& pred )
            {
                auto const right_lineals = view2::right_lineal.fetch( ctx, node );
                auto const ns = anchor::root( node ) | pred | act::to_fetch_set( ctx );

                return rvs::set_intersection( right_lineals, ns )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const right_lineals = [ & ] // Note: set_intersection requires two sorted ranges.
                {
                    auto t = view2::right_lineal.fetch( ctx, node ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();
                auto const ns = [ & ] // Note: set_intersection requires two sorted ranges.
                {
                    auto t = pred | act::to_fetch_set( ctx ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();

                // Note: set_intersection requires two sorted ranges.
                return rvs::set_intersection( right_lineals, ns )
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto rs = anchor::root( node )
                | view2::desc
                | act::to_fetch_set( ctx );

        rs.emplace( LinkNode{ .id = node } );

        return rs;
    }
}

SCENARIO( "view::RightLineal::fetch", "[node_view][right_lineal]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "right_lineal" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

        THEN( "view::right_lineal" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n1 ) | view2::right_lineal | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::right_lineal( <heading> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n1 ) | view2::right_lineal( "root" ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::right_lineal( Uuid )" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n1 ) | view2::right_lineal( root ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::right_lineal( <Tether> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n1 ) | view2::right_lineal( anchor::root( n1 ) | view2::parent ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
    }
}

auto RightLineal::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< RightLineal >();
}

auto RightLineal::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& pred ){ return fmt::format( "right_lineal( '{}' )", pred ); }
        ,   [ & ]( LinkPtr const& pred ){ return fmt::format( "right_lineal( '{}' )", *pred | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "right_lineal";
    }
}

auto RightLineal::compare_less( Link const& other ) const
    -> bool
{ 
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return pred_ < dynamic_cast< decltype( *this )& >( other ).pred_;
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
