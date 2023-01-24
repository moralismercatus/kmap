/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_FETCH_SET_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_FETCH_SET_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct ToFetchSet // For internal use. Used by other views to propagate fetch results.
{
    FetchContext const& ctx;
};

auto to_fetch_set( FetchContext const& ctx )
    -> ToFetchSet;

auto operator|( Tether const& lhs
              , ToFetchSet const& rhs )
    -> FetchSet;

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_FETCH_SET_HPP
