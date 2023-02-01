/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/resolve.hpp"

#include "com/network/network.hpp"
#include "contract.hpp"
#include "path/view/act/abs_path.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Resolve::create( CreateContext const& ctx
                   , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "This is not what you want" );
}

auto Resolve::fetch( FetchContext const& ctx
                   , Uuid const& node ) const
    -> FetchSet
{
    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred )
            {
                return view2::resolve( std::string{ pred } ).fetch( ctx, node );
            }
        ,   [ & ]( std::string const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const rnode = nw->resolve( node );

                if( pred == KTRYE( nw->fetch_heading( rnode ) ) )
                {
                    return FetchSet{ LinkNode{ .id = rnode } };
                }
                else
                {
                    return FetchSet{};
                }
            }
        ,   [ & ]( Uuid const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );

                if( nw->resolve( node ) == pred )
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
                auto const resolutions = view2::resolve.fetch( ctx, node );

                return resolutions
                     | rvs::filter( [ & ]( auto const& c ){ return anchor::node( c.id ) | pred | act2::exists( ctx.km ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const resolutions = [ & ] // Note: set_intersection requires two sorted ranges.
                {
                    auto t = view2::resolve.fetch( ctx, node ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();
                auto const ns = [ & ] // Note: set_intersection requires two sorted ranges.
                {
                    auto t = pred | act::to_fetch_set( ctx ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();

                fmt::print( "[resolve] Tether: {}\n", pred | act::to_string );

                for( auto const& e : resolutions )
                {
                    fmt::print( "[resolve][resolution] {}\n", KTRYE( anchor::abs_root | view2::desc( e.id ) | act::abs_path_flat( ctx.km ) ) );
                }
                for( auto const& e : ns )
                {
                    fmt::print( "[resolve][n] {}\n", KTRYE( anchor::abs_root | view2::desc( e.id ) | act::abs_path_flat( ctx.km ) ) );
                }
                for( auto const& e : rvs::set_intersection( resolutions, ns )
                                   | ranges::to< FetchSet >() )
                {
                    fmt::print( "[resolve] {}: {}\n", pred | act::to_string, anchor::node( e.id ) | act::abs_path_flat( ctx.km ) );
                }

                return rvs::set_intersection( resolutions, ns )
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );

        return FetchSet{ LinkNode{ .id = nw->resolve( node ) } };
    }
}

auto Resolve::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Resolve >();
}

auto Resolve::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "resolve( '{}' )", arg ); }
        ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "resolve( {} )", ( *arg ) | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "resolve";
    }
}

auto Resolve::compare_less( Link const& other ) const
    -> bool
{
    auto const cmp = compare_links( *this, other );

    return ( cmp == std::strong_ordering::less );
}

} // namespace kmap::view2
