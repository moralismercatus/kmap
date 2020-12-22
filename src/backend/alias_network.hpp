
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_BE_ALIAS_NETWORK_HPP
#define KMAP_BE_ALIAS_NETWORK_HPP

#include "basic_network.hpp"
#include "../common.hpp"
#include "../container.hpp"

namespace kmap::backend {

/// BasicNetwork + Aliasing
class AliasNetwork 
{
public:
    AliasNetwork( Uuid const& root );

    [[ nodiscard ]]
    auto child_exists( Uuid const& child ) const
        -> bool;
    [[ nodiscard ]]
    auto child_exists( Uuid const& parent
                     , Uuid const& child ) const
        -> bool;
    [[ nodiscard ]]
    auto child_iterator_cbegin() const
        -> IdIdsSet::const_iterator;        
    [[ nodiscard ]]
    auto child_iterator_cend() const
        -> IdIdsSet::const_iterator;        
    auto create_child( Uuid const& parent
                     , Uuid const& child )
        -> Result< Uuid >;
    // TODO: Could use a view concept in which copy() accepts a view of a graph, and always uses the root. e.g.: make_view( BasicNetwork const&, root )...
    auto copy( Uuid const& parent
             , BasicNetwork const& other
             , Uuid const& other_root )
        -> Result< void >;
    auto copy_descendants( Uuid const& parent
                         , BasicNetwork const& other
                         , Uuid const& other_root )
        -> Result< void >;
    auto delete_node( Uuid const& node )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto exists( Uuid const& node ) const
        -> bool;
    auto fetch_children( Uuid const& parent ) const
        -> Result< std::set< Uuid > >;
    auto fetch_children() const
        -> IdIdsSet const&;
    [[ nodiscard ]]
    auto fetch_parent( Uuid const& child ) const
        -> Result< Uuid >;
    [[ maybe_unused ]]
    auto move_node( Uuid const& src
                  , Uuid const& dst )
        -> Result< void >;
    [[ nodiscard ]]
    auto aliases() const
        -> UuidSet;

private:
    BasicNetwork bn_;
    IdIdsSet aliases_ = {};
};

auto fetch_leaves( BasicNetwork const& network
                 , Uuid const& root )
    -> Result< UuidSet >;
auto fetch_lineage( BasicNetwork const& network
                  , Uuid const& node )
    -> Result< UuidVec >;

} // namespace kmap::backend

#endif // KMAP_BE_ALIAS_NETWORK_HPP