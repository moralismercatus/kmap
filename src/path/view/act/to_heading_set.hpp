/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_TO_HEADING_SET_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_TO_HEADING_SET_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct ToHeadingSet
{
    Kmap const& km;
};

auto to_heading_set( Kmap const& km )
    -> ToHeadingSet;

auto operator|( Tether const& lhs
              , ToHeadingSet const& rhs )
    -> std::set< std::string >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_TO_HEADING_SET_HPP
