/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/anchor/root.hpp"

#include "common.hpp"
#include "kmap.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::view2::anchor {

auto Root::fetch( FetchContext const& ctx ) const
    -> FetchSet
{
    return nodes
         | rvs::transform( [ & ]( auto const& e ){ return LinkNode{ .id = e }; } )
         | ranges::to< FetchSet >();
}

auto Root::to_string() const
    -> std::string
{
    auto const flattened = nodes
                         | rvs::transform( []( auto const& e ){ return fmt::format( "'{}'", kmap::to_string( e ) ); } )
                         | rvs::join( ',' )
                         | ranges::to< std::string >();

    return fmt::format( "root( {} )", flattened );
}

auto Root::compare_less( Anchor const& other ) const
    -> bool
{
    auto const cmp = compare_anchors( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return nodes < dynamic_cast< decltype( *this )& >( other ).nodes;
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

// auto root( Tether const& tether )
//     -> Tether
// {
//     KMAP_THROW_EXCEPTION_MSG( "TODO: Allow variant for root to be UuidSet or Tether" );
//     // return Tether{ .anchor = std::make_shared< Root >( tether ) };
// }

auto root( Uuid const& node )
    -> Root
{
    return Root{ UuidSet{ node } };
}

auto root( UuidSet const& nodes )
    -> Root
{
    return Root{ nodes };
}

} // namespace kmap::view2::anchor
