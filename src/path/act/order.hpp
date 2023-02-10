/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_ORDER_HPP
#define KMAP_PATH_ACT_ORDER_HPP

#include "com/network/network.hpp"
#include "contract.hpp"
#include "error/result.hpp"
#include "kmap.hpp"
#include "util/concepts.hpp"
#include "utility.hpp"

#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

#include <map>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct Order
{
    Kmap const& kmap;

    Order( Kmap const& km ) : kmap{ km } {}
};

auto order( Kmap const& kmap )
    -> Order;

template< concepts::Range RT >
auto operator|( RT const& range, Order const& op )
    -> UuidVec
{
    namespace rvs = ranges::views;

    using OrderedChildren = std::map< Uuid, std::vector< Uuid > >;

    auto ordered_children_map = OrderedChildren{};
    auto const nw = KTRYE( op.kmap.fetch_component< com::Network >() );
    auto const left_lineage = [ & ]( auto const& node ) -> std::vector< Uuid >
    {
        auto ll = std::vector< Uuid >{ node };
        auto parent = nw->fetch_parent( node );
        while( parent )
        {
            ll.emplace_back( parent.value() );
            parent = nw->fetch_parent( parent.value() );
        }
        return ll | rvs::reverse | ranges::to< std::vector >();
    };
    auto const lineage_map = range
                           | rvs::transform( [ & ]( auto const& e ){ return std::pair{ e, left_lineage( e ) }; } )
                           | ranges::to< std::map >();
    auto const is_sibling_ordered = [ & ]( auto const& lhs, auto const& rhs )
    {
        BC_ASSERT( nw->is_sibling( lhs, rhs ) );
        BC_ASSERT( lhs != rhs );
        (void)lhs;
        (void)rhs;
        auto const rlhs = lhs;//nw->resolve( lhs );
        auto const rrhs = rhs;//nw->resolve( rhs );
        // BC_ASSERT( rlhs != rrhs );

        auto const parent = KTRYE( nw->fetch_parent( rlhs ) );

        if( !ordered_children_map.contains( parent ) )
        {
            ordered_children_map.emplace( parent, KTRYE( nw->fetch_children_ordered( parent ) ) );
        }

        auto const it = ordered_children_map.find( parent ); BC_ASSERT( it != ordered_children_map.end() );
        auto const& ochildren = it->second;
        auto const lcit = ranges::find( ochildren, rlhs ); BC_ASSERT( lcit != ochildren.end() );
        auto const rcit = ranges::find( ochildren, rrhs ); BC_ASSERT( rcit != ochildren.end() );
        auto const cbit = ochildren.begin(); BC_ASSERT( cbit != ochildren.end() );

        return std::distance( cbit, lcit ) < std::distance( cbit, rcit );
    };
    auto const compare = [ & ]( auto const& lhs, auto const& rhs )
    { 
        BC_ASSERT( lhs != rhs );

        if( nw->is_sibling( lhs, rhs ) )
        {
            return is_sibling_ordered( lhs, rhs );
        }
        else
        {
            auto const lhs_lin = lineage_map.at( lhs );
            auto const rhs_lin = lineage_map.at( rhs );
            auto lit = lhs_lin.begin();
            auto rit = rhs_lin.begin();

            while( lit != lhs_lin.end()
                && rit != rhs_lin.end()
                && *lit == *rit )
            {
                BC_ASSERT( lit != lhs_lin.end() && rit != rhs_lin.end() );
                
                ++lit;
                ++rit;
            }

            if( lit == lhs_lin.end() ) // lhs precedes rhs
            {
                BC_ASSERT( rit != rhs_lin.end() );

                return true;
            }
            else if( rit == rhs_lin.end() ) // rhs precedes lhs
            {
                BC_ASSERT( lit != lhs_lin.end() );

                return false;
            }
            else// if( nw->is_sibling( *lit, *rit ) )
            {
                BC_ASSERT( nw->is_sibling( *lit, *rit ) );

                return is_sibling_ordered( *lit, *rit );
            }
        }
    };
    using OrderedNodes = std::set< Uuid, decltype( compare ) >;

    auto ordered_rng = OrderedNodes{ compare };

    for( auto const& n : range ) { ordered_rng.emplace( n ); }

    return ordered_rng 
         | ranges::to< UuidVec >();
}

// TODO: Provide Result<> version that just calls non-Result version (try/catch => Result)?
// template< concepts::Range RT >
// auto operator|( Result< RT > const& range, Order const& op )
//     -> Result< UuidVec >;
// TODO: Likewise, could also provide a std::set => act::order => std::vector if I make a !concepts::SequenceRange (vector => SequenceContainer) overload.

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_ACT_ORDER_HPP
