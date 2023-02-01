/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ANCHOR_NODE_HPP
#define KMAP_PATH_NODE_VIEW2_ANCHOR_NODE_HPP

#include "common.hpp"
#include "path/view/anchor/anchor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::anchor {

class Node : public Anchor
{
    UuidSet nodes; // TODO: I suspect I'll want something like `std::set< view2::Node >`, such that constraints can be stored in the "node" to reduce redundant computation.

public:
    Node( UuidSet const& ns )
        : nodes{ ns }
    {
    }
    virtual ~Node() = default;

    auto clone() const -> std::unique_ptr< Anchor > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto fetch( FetchContext const& ctx ) const -> FetchSet override;
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Anchor const& anchor ) const -> bool override;
};

// TODO: Misnomer, actually, because we can go "backwards" from the node via view::parent, view::ancestor, etc. 
//       Maybe more clearly named "anchor::node"?
// auto node( Tether const& tether ) -> Node;
// TODO: variadic template... instead of single node.
auto node( Uuid const& n ) -> Node;
// TODO: range concept instead of UuidSet arg.
auto node( UuidSet const& ns ) -> Node;

} // namespace kmap::view2::anchor

#endif // KMAP_PATH_NODE_VIEW2_ANCHOR_NODE_HPP
