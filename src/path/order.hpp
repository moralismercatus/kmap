/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ORDER_HPP
#define KMAP_PATH_ORDER_HPP

#include <com/network/network.hpp>
#include <contract.hpp>
#include <error/result.hpp>
#include <kmap.hpp>
#include <util/concepts.hpp>
#include <utility.hpp>
#include <path/ancestry.hpp>

#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

#include <map>

namespace kmap::path::detail {

auto order( Kmap const& km
          , UuidVec& ordered_nodes
          , Uuid const& root
          , std::vector< UuidVec > const& lineages )
    -> Result< void >;

} // namespace kmap::path::detail

namespace kmap::path {

template< concepts::Range RT >
auto order( Kmap const& km
          , RT const& range )
    -> Result< UuidVec >
{
    namespace rvs = ranges::views;

    KM_RESULT_PROLOG();

    if( range.empty() )
    {
        return UuidVec{};
    }

    KMAP_ENSURE( ( range.size() == UuidSet{ range.begin(), range.end() }.size() ), error_code::network::duplicate_node ); // No duplicate nodes allowed.

    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto ordered_nodes = std::vector< Uuid >{};
    auto const left_lineages = KTRY( [ & ]() -> Result< std::vector< UuidVec > >
    {
        auto t = std::vector< UuidVec >{};
        for( auto const& n : range )
        {
            auto lin = KTRY( fetch_ancestry_ordered( km, n ) );
            lin.emplace_back( n );
            t.emplace_back( lin );
        }
        return t;
    }() );
    auto const common_root = left_lineages.at( 0 ).front();
    auto const sub_lineages = KTRY( [ & ]() -> Result< decltype( left_lineages ) >
    {
        auto t = decltype( left_lineages ){};
        for( auto const& lin : left_lineages )
        {
            KMAP_ENSURE( lin.at( 0 ) == common_root, error_code::common::uncategorized );
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
    }() );

    // So, at this point, all sub_lineages at [0] are siblings to the common root:
    KTRY( detail::order( km, ordered_nodes, common_root, sub_lineages ) );

    return ordered_nodes;
}

} // namespace kmap::path

#endif // KMAP_PATH_ORDER_HPP
