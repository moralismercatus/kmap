/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_CREATE_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_CREATE_HPP

#include <common.hpp>
#include <kmap_fwd.hpp>
#include <path/view/act/actor.hpp>
#include <path/view/tether.hpp>

namespace kmap::view2::act {

struct Create : public Actor
{
    Kmap& km;

    Create( Kmap& k );
};

auto create( Kmap& km )
    -> Create;

auto operator|( Tether const& lhs
              , Create const& rhs )
    -> Result< UuidSet >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_CREATE_HPP
