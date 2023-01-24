/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_FETCH_NODE_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_FETCH_NODE_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/actor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct FetchNode : public Actor
{
    Kmap const& km;

    FetchNode( Kmap const& k );
};
struct FetchNodeCount : public Actor
{
    Kmap const& km;
    unsigned const& count;
};

auto fetch_node( Kmap const& km )
    -> FetchNode;
auto fetch_node_count( Kmap const& km
                     , unsigned const& count )
    -> FetchNodeCount;

auto operator|( Tether const& lhs
              , FetchNode const& rhs )
    -> Result< Uuid >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_FETCH_NODE_HPP
