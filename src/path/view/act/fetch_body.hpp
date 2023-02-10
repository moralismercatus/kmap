/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_FETCH_BODY_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_FETCH_BODY_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/actor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct FetchBody : public Actor
{
    Kmap const& km;

    FetchBody( Kmap const& k );
};

auto fetch_body( Kmap const& km )
    -> FetchBody;

auto operator|( Tether const& lhs
              , FetchBody const& rhs )
    -> Result< std::string >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_FETCH_BODY_HPP
