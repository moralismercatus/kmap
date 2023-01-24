/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_ACTOR_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_ACTOR_HPP

#include "path/view/anchor/anchor.hpp"
#include "path/view/link.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

class Actor
{
};

template< typename AnchorType
        , typename TailLink
        , typename ActorType >
    requires std::derived_from< AnchorType, Anchor >
          && std::derived_from< TailLink, Link >
auto operator|( TetherCT< AnchorType, TailLink > const& ctc
              , ActorType const& actor )
{
    return ctc | to_tether | actor;
}

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_ACTOR_HPP
