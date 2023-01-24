/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_COUNT_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_COUNT_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct Count
{
    Kmap const& km;
};

auto count( Kmap const& km )
    -> Count;

auto operator|( Tether const& lhs
              , Count const& rhs )
    -> unsigned;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_COUNT_HPP
