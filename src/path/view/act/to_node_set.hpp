/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_TO_NODE_SET_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_TO_NODE_SET_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct ToNodeSet
{
    Kmap const& km;
};

auto to_node_set( Kmap const& km )
    -> ToNodeSet;

auto operator|( Tether const& lhs
              , ToNodeSet const& rhs )
    -> UuidSet;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_TO_NODE_SET_HPP
