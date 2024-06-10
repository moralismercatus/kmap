/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_FETCH_TITLE_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_FETCH_TITLE_HPP

#include <common.hpp>
#include <kmap_fwd.hpp>
#include <path/view/act/actor.hpp>
#include <path/view/tether.hpp>

namespace kmap::view2::act {

struct FetchTitle : public Actor
{
    Kmap const& km;

    FetchTitle( Kmap const& k );
};

auto fetch_title( Kmap const& km )
    -> FetchTitle;

auto operator|( Tether const& lhs
              , FetchTitle const& rhs )
    -> Result< std::string >;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_FETCH_TITLE_HPP
