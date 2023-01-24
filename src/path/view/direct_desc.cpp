/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/direct_desc.hpp"

#include "com/network/network.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "path/view/desc.hpp"
#include "util/result.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

#include <string>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto DirectDesc::create( CreateContext const& ctx
                       , Uuid const& root ) const
    -> Result< UuidSet >
{
    return result::make_result< UuidSet >();
}

auto DirectDesc::fetch( FetchContext const& ctx
                      , Uuid const& node ) const
    -> FetchSet
{
    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred )
            {
                return view2::desc( std::string{ pred } ).fetch( ctx, node );
            }
        ,   [ & ]( std::string const& pred )
            {
                auto const make_direct = []( std::string const& s ){ return fmt::format( ".{}", s ); };
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const tform = [ & ]( auto const& c )
                {
                    return LinkNode{ .id = c }; 
                };

                if( auto const dr = decide_path( ctx.km, node, node, make_direct( pred ) )
                    ; dr )
                {
                    auto const& d = dr.value();

                    return d
                         | rvs::transform( tform )
                         | ranges::to< FetchSet >();
                }
                else
                {
                    return FetchSet{};
                }
            }
        ,   [ & ]( Uuid const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                // TODO: More efficient to check if node == fetch_parent( pred ): return pred;
                auto const children = nw->fetch_children( node );
                auto const tform = [ & ]( auto const& c )
                {
                    return LinkNode{ .id = c }; 
                };
                return children
                     | rvs::filter( [ & ]( auto const& e ){ return pred == e; } )
                     | rvs::transform( tform )
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

auto DirectDesc::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< DirectDesc >();
}

auto DirectDesc::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "direct_desc( '{}' )", arg ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "direct_desc";
    }
}

auto DirectDesc::compare_less( Link const& other ) const
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
