/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_ERASE_NODE_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_ERASE_NODE_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/actor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct EraseNode : public Actor
{
    Kmap& km;

    EraseNode( Kmap& k );
};

auto erase_node( Kmap& km )
    -> EraseNode;

auto operator|( Tether const& lhs
              , EraseNode const& rhs )
    -> Result< void >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_ERASE_NODE_HPP
