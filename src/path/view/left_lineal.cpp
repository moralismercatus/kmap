/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/left_lineal.hpp"

#include "com/network/network.hpp"
#include "path.hpp"
#include "path/view/ancestor.hpp"
#include "path/view/anchor/node.hpp"
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

auto LeftLineal::create( CreateContext const& ctx
                       , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "this is not what you want" );
}

auto LeftLineal::fetch( FetchContext const& ctx
                      , Uuid const& node ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred ) -> Result< FetchSet >
            {
                return KTRY( view2::left_lineal( std::string{ pred } ).fetch( ctx, node ) );
            }
        ,   [ & ]( std::string const& pred ) -> Result< FetchSet >
            {
                auto rs = FetchSet{};
                auto const left_lineals = KTRY( view2::left_lineal.fetch( ctx, node ) );
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

                return left_lineals
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

                if( is_lineal( *nw, pred, node ) )
                {
                    return FetchSet{ LinkNode{ .id = pred } };
                }
                else
                {
                    return FetchSet{};
                }
            }
        ,   [ & ]( LinkPtr const& pred ) -> Result< FetchSet >
            {
                auto const left_lineals = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::left_lineal.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( anchor::node( node ) | pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( left_lineals, ns )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const left_lineals = KTRY( [ & ]() -> Result< std::vector< LinkNode > > // Note: set_intersection requires two sorted ranges.
                {
                    auto t = KTRY( view2::left_lineal.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                // Note: set_intersection requires two sorted ranges.
                return rvs::set_intersection( left_lineals, ns )
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto rs = KTRY( anchor::node( node )
                      | view2::ancestor
                      | act::to_fetch_set( ctx ) );

        rs.emplace( LinkNode{ .id = node } );

        return rs;
    }
}

SCENARIO( "view::LeftLineal::fetch", "[node_view][left_lineal]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "left_lineal" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

        THEN( "view::left_lineal" )
        {
            auto const fn = anchor::node( n1 ) | view2::left_lineal | act::to_node_set( km );
            REQUIRE( fn == UuidSet{ root, n1 } );
        }
        THEN( "view::left_lineal( <heading> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::left_lineal( "root" ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::left_lineal( Uuid )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::left_lineal( root ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::left_lineal( <Tether> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::left_lineal( anchor::node( n1 ) | view2::parent ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
    }
}

auto LeftLineal::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< LeftLineal >();
}

auto LeftLineal::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& pred ){ return fmt::format( "left_lineal( '{}' )", pred ); }
        ,   [ & ]( LinkPtr const& pred ){ return fmt::format( "left_lineal( '{}' )", *pred | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "left_lineal";
    }
}

auto LeftLineal::compare_less( Link const& other ) const
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
