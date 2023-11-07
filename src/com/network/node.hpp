/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_NETWORK_NODE_HPP
#define KMAP_NETWORK_NODE_HPP

#include <common.hpp>

namespace kmap::com {

// TODO:
// So, the question about this type is whether it should be constrained. A "node" must have an ID and heading (it can derive a title from heading).
// The problem is that I had initially desired to use this type as an argument for node_view, meaning what I really want to say is "predicate based on id, heading, or title",
// so maybe that type belongs specifically in view: view2::node( variant_type< id, Heading, or Type > )
struct Node
{
    Uuid id = {};
    std::string heading = {};
    std::string title = {};

    Node( Uuid i ) : id{ id } {}
    Node( std::string const& h ) : heading{ h } {}
    Node( std::string const& t ) : heading{ t } {}
};

} // namespace kmap::com

#endif // KMAP_NETWORK_NODE_HPP
