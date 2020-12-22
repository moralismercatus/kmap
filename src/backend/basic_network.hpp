
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_BE_BASIC_NETWORK_HPP
#define KMAP_BE_BASIC_NETWORK_HPP

#include "../common.hpp"
#include "../container.hpp"

namespace kmap::backend {

// The idea behind this is the simplest form of a network: no headings, nor titles, nor aliases.
// Just a set of parents, children, and a root. This class can be extended to accomodate more
// capabilities, even node payloads other than headings or titles. One extension should be
// the "Kmap"/bridge/combined(font,back end) extension that propagates to DB and JS interface. 
// Note: For adding data/payload, consider that it may be well to consider Boost.Graph's approach of separating the data into a separate type that can be queried by node ID.
//       Effectively, this could mean BasicNetwork< DataType > to specify various kinds of Networks, based on the data that is associated with each node.
// TODO: Ideally, the ID would be a template parameter, allowing the user to choose how to uniquely identify a node. For example, uint64_t being used for memory analysis indices.
class BasicNetwork
{
public:
    BasicNetwork( Uuid const& root ); // Warning: iterators assume root() always exists, so if you change assumption, you must redesign the iterators.

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
    auto nodes() const
        -> UuidSet;
    [[ nodiscard ]]
    auto root() const
        -> Uuid;

private:
    Uuid root_;
    IdIdsSet children_ = {};
};

auto fetch_leaves( BasicNetwork const& network
                 , Uuid const& root )
    -> Result< UuidSet >;
auto fetch_lineage( BasicNetwork const& network
                  , Uuid const& node )
    -> Result< UuidVec >;

} // namespace kmap::backend

#endif // KMAP_BE_BASIC_NETWORK_HPP