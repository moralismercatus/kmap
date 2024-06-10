/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_FETCH_OR_CREATE_NODE_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_FETCH_OR_CREATE_NODE_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/actor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct FetchOrCreateNode : public Actor
{
    Kmap& km;

    FetchOrCreateNode( Kmap& k );
};

auto fetch_or_create_node( Kmap& km )
    -> FetchOrCreateNode;

auto operator|( Tether const& lhs
              , FetchOrCreateNode const& rhs )
    -> Result< UuidSet >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_FETCH_OR_CREATE_NODE_HPP
