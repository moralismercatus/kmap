/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_EXISTS_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_EXISTS_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct Exists
{
    Kmap const& km;
};

auto exists( Kmap const& km )
    -> Exists;

auto operator|( Tether const& lhs
              , Exists const& rhs )
    -> bool;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_EXISTS_HPP
