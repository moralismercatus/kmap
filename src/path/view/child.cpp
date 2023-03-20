/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/child.hpp"

#include "com/network/network.hpp"
#include "util/result.hpp"
#include <contract.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <compare>
#include <variant>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Child::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
    
    auto rv = result::make_result< UuidSet >();

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred ) -> Result< UuidSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const c = KTRY( nw->create_child( root, pred ) );

                return UuidSet{ c };
            }
        ,   [ & ]( std::string const& pred ) -> Result< UuidSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const c = KTRY( nw->create_child( root, pred ) );

                return UuidSet{ c };
            }
        ,   [ & ]( Uuid const& pred ) -> Result< UuidSet >
            {
                KMAP_THROW_EXCEPTION_MSG( "invalid node qualifier" );
            }
        ,   [ & ]( LinkPtr const& pred ) -> Result< UuidSet >
            {
                KMAP_THROW_EXCEPTION_MSG( "TODO: Impl." );
            }
        };

        rv = KTRY( std::visit( dispatch, pred_.value() ) );
    }

    return rv;
}

auto Child::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> FetchSet
{
    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred )
            {
                return view2::child( std::string{ pred } ).fetch( ctx, node );
            }
        ,   [ & ]( std::string const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const children = view2::child.fetch( ctx, node );

                return children
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred )
            {
                auto const children = view2::child.fetch( ctx, node );

                return children
                     | rvs::filter( [ & ]( auto const& e ){ return pred == e.id; } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( LinkPtr const& pred )
            {
                auto const children = view2::child.fetch( ctx, node );

                return children 
                     | rvs::filter( [ & ]( auto const& c ){ return anchor::node( c.id ) | pred | act2::exists( ctx.km ); } )
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
             | rvs::transform( []( auto const& c ){ return LinkNode{ .id = c }; } )
             | ranges::to< FetchSet >();
    }
}

auto Child::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Child >();
}

auto Child::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "child( '{}' )", arg ); }
        ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "child( {} )", ( *arg ) | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "child";
    }
}

auto Child::compare_less( Link const& other ) const
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

SCENARIO( "Child::compare_less", "[node_view]" )
{
    GIVEN( "two child views defined with same predicates" )
    {
        auto const c1 = view2::child( "1" );
        auto const c1b = view2::child( "1" );
        Link const& l1 = c1;

        REQUIRE( !( c1 < c1 ) );
        REQUIRE( !( c1 < c1b ) );
        REQUIRE( !( c1b < c1 ) );
        REQUIRE( !( l1 < c1 ) );
        REQUIRE( !( l1 < c1b ) );
        REQUIRE( !( c1 < l1 ) );
        REQUIRE( !( c1b < l1 ) );
    }
}

} // namespace kmap::view2