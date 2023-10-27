/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_TO_NODE_VEC_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_TO_NODE_VEC_HPP

#include <common.hpp>
#include <kmap_fwd.hpp>
#include <path/view/tether.hpp>

namespace kmap::view2::act {

struct ToNodeVec
{
    Kmap const& km;
};

auto to_node_vec( Kmap const& km )
    -> ToNodeVec;

auto operator|( Tether const& lhs
              , ToNodeVec const& rhs )
    -> UuidVec;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_TO_NODE_VEC_HPP
