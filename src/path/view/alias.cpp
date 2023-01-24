/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/alias.hpp"

#include "com/network/network.hpp"
#include "kmap.hpp"
#include "path/view/act/fetch_node.hpp"
#include "path/view/act/to_string.hpp"
#include "path/view/ancestor.hpp"
#include "path/view/anchor/root.hpp"
#include "path/view/parent.hpp"
#include "path/view/resolve.hpp"
#include "path/view/tether.hpp"
#include "test/util.hpp"

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

auto Alias::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "this is not what you want" );
}

auto Alias::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> FetchSet
{
    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred )
            {
                return view2::alias( std::string{ pred } ).fetch( ctx, node );
            }
        ,   [ & ]( std::string const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const aliases = view2::alias.fetch( ctx, node );

                return aliases
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const aliases = view2::alias.fetch( ctx, node );

                return aliases
                     | rvs::filter( [ & ]( auto const& c ){ return c.id == pred; } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( LinkPtr const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const aliases = view2::alias.fetch( ctx, node );

                return aliases
                     | rvs::filter( [ & ]( auto const& c ){ return anchor::root( c.id ) | pred | act2::exists( ctx.km ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const aliases = [ & ] // Note: set_intersection requires two sorted ranges.
                {
                    auto t = view2::alias.fetch( ctx, node ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();
                auto const ns = [ & ] // Note: set_intersection requires two sorted ranges.
                {
                    auto t = pred | act::to_fetch_set( ctx ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();

                return rvs::set_intersection( aliases, ns )
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
        auto const children = nw->fetch_children( node );

        return children
             | rvs::filter( [ & ]( auto const& c ){ return nw->is_alias( c ); } )
             | rvs::transform( [ & ]( auto const& a ){ return LinkNode{ .id = a }; } )
             | ranges::to< FetchSet >();
    }
}

SCENARIO( "view::Alias::fetch", "[node_view][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "alias" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

        THEN( "view::alias" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n2 ) | view2::alias | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( <heading> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n2 ) | view2::alias( "1" ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( Uuid )" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n2 ) | view2::alias( a21 ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( Link )" )
        {
            auto const fn = REQUIRE_TRY( anchor::root( n2 ) | view2::alias( view2::resolve( n1 ) | view2::parent( root ) ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
    }
}

auto Alias::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Alias >();
}

auto Alias::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "alias( '{}' )", arg ); }
        ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "alias( {} )", ( *arg ) | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "alias";
    }
}

auto Alias::compare_less( Link const& other ) const
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