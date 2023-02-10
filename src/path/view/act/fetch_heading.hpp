/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_FETCH_HEADING_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_FETCH_HEADING_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/actor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct FetchHeading : public Actor
{
    Kmap const& km;

    FetchHeading( Kmap const& k );
};

auto fetch_heading( Kmap const& km )
    -> FetchHeading;

auto operator|( Tether const& lhs
              , FetchHeading const& rhs )
    -> Result< std::string >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_FETCH_HEADING_HPP
