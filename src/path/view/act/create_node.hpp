/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_CREATE_NODE_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_CREATE_NODE_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/actor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct CreateNode : public Actor
{
    Kmap& km;

    CreateNode( Kmap& k );
};

auto create_node( Kmap& km )
    -> CreateNode;

auto operator|( Tether const& lhs
              , CreateNode const& rhs )
    -> Result< Uuid >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_CREATE_NODE_HPP
