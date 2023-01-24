/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_ABS_PATH_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_ABS_PATH_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct AbsPathFlat
{
    Kmap const& km;
};

auto abs_path_flat( Kmap const& km )
    -> AbsPathFlat;

auto operator|( Tether const& lhs
              , AbsPathFlat const& rhs )
    -> Result< std::string >;
// auto operator|( Anchor const& lhs
//               , AbsPathFlat const& rhs )
//     -> Result< std::string >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_ABS_PATH_HPP
