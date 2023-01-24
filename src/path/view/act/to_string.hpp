/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_TO_STRING_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_TO_STRING_HPP

#include "common.hpp"
#include "kmap_fwd.hpp"
#include "path/view/act/to_string.hpp"
#include "path/view/anchor/anchor.hpp"
#include "path/view/link.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::act {

struct ToString {};

auto const to_string = ToString{};

auto operator|( Anchor const& lhs
              , ToString const& rhs )
    -> std::string;
auto operator|( Link const& lhs
              , ToString const& rhs )
    -> std::string;
auto operator|( Tether const& lhs
              , ToString const& rhs )
    -> std::string;
template< typename AnchorType
        , typename TailLink >
    requires std::derived_from< AnchorType, Anchor >
          && std::derived_from< TailLink, Link >
auto operator|( TetherCT< AnchorType, TailLink > const& lhs
              , ToString const& rhs )
    -> std::string
{
    return fmt::format( "TetherCT:\n\tanchor:{}\n\tchain:{}\n"
                      , lhs.anchor | act::to_string
                      , lhs.tail_link | act::to_string );
}

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_TO_STRING_HPP
