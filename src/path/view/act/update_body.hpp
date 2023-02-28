/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_UPDATE_BODY_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_UPDATE_BODY_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/actor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct UpdateBody : public Actor
{
    Kmap& km;
    std::string const content;

    UpdateBody( Kmap& k
              , std::string const& content );
};

auto update_body( Kmap& km
                , std::string const& content )
    -> UpdateBody;

auto operator|( Tether const& lhs
              , UpdateBody const& rhs )
    -> Result< void >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_UPDATE_BODY_HPP
