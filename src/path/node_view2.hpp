/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_HPP
#define KMAP_PATH_NODE_VIEW2_HPP

#include <path/view/act/abs_path.hpp>
#include <path/view/act/count.hpp>
#include <path/view/act/create.hpp>
#include <path/view/act/create_node.hpp>
#include <path/view/act/erase_node.hpp>
#include <path/view/act/exists.hpp>
#include <path/view/act/fetch_body.hpp>
#include <path/view/act/fetch_heading.hpp>
#include <path/view/act/fetch_node.hpp>
#include <path/view/act/fetch_or_create_node.hpp>
#include <path/view/act/fetch_title.hpp>
#include <path/view/act/single.hpp>
#include <path/view/act/to_fetch_set.hpp>
#include <path/view/act/to_heading_set.hpp>
#include <path/view/act/to_node_set.hpp>
#include <path/view/act/to_node_vec.hpp>
#include <path/view/act/to_string.hpp>
#include <path/view/act/update_body.hpp>
#include <path/view/alias.hpp>
#include <path/view/all_of.hpp>
#include <path/view/ancestor.hpp>
#include <path/view/anchor/abs_root.hpp>
#include <path/view/anchor/anchor.hpp>
#include <path/view/anchor/anchor.hpp>
#include <path/view/anchor/node.hpp>
#include <path/view/any_of.hpp>
#include <path/view/attr.hpp>
#include <path/view/child.hpp>
#include <path/view/desc.hpp>
#include <path/view/direct_desc.hpp>
#include <path/view/exactly.hpp>
#include <path/view/left_lineal.hpp>
#include <path/view/link.hpp>
#include <path/view/none_of.hpp>
#include <path/view/order.hpp>
#include <path/view/parent.hpp>
#include <path/view/resolve.hpp>
#include <path/view/right_lineal.hpp>
#include <path/view/root.hpp>
#include <path/view/sibling.hpp>

#include <concepts>

namespace kmap::view2 {

// TODO: Why are these placed here and not tether.hpp?
template< typename AnchorType
        , typename TailLink >
    requires std::derived_from< AnchorType, Anchor >
          && std::derived_from< TailLink, Link >
auto operator|( TetherCT< AnchorType, TailLink > const& tct
              , ToTether const& )
    -> Tether
{
    return Tether{ PolymorphicValue< Anchor >{ std::make_unique< AnchorType >( tct.anchor ) }
                 , Link::LinkPtr{ std::make_unique< TailLink >( tct.tail_link ) } };
}

template< typename AnchorType
        , typename TailLink >
    requires std::derived_from< AnchorType, Anchor >
          && std::derived_from< TailLink, Link >
auto operator|( AnchorType const& anchor
              , TailLink const& link )
    -> TetherCT< AnchorType, TailLink >
{
    return TetherCT< AnchorType, TailLink >{ anchor, link };
}

template< typename AnchorType
        , typename TailLink
        , typename NextLink >
    requires std::derived_from< AnchorType, Anchor >
          && std::derived_from< TailLink, Link >
          && std::derived_from< NextLink, Link >
auto operator|( TetherCT< AnchorType, TailLink > const& tether
              , NextLink const& link )
    -> TetherCT< AnchorType, NextLink >
{
    auto nlink = link;

    if( auto prev = std::ref( nlink.prev() )
      ; prev.get() )
    {
        auto last = prev;

        while( prev.get() )
        {
            last = prev;
            prev = std::ref( prev.get()->prev() );
        }

        last.get()->prev( { std::make_unique< TailLink >( tether.tail_link ) } );
    }
    else
    {
        nlink.prev( { std::make_unique< TailLink >( tether.tail_link ) } );
    }

    return TetherCT< AnchorType, NextLink >{ tether.anchor, nlink };
}

template< typename AnchorType >
    requires std::derived_from< AnchorType, Anchor >
auto operator|( AnchorType const& anchor
              , Link::LinkPtr const& link )
    -> Tether
{
    return Tether{ Anchor::AnchorPtr{ std::make_unique< AnchorType >( anchor ) }, link };
}

} // namespace kmap::view2

namespace kmap
{
    namespace act2 = view2::act;
    namespace anchor = view2::anchor;
}

#endif // KMAP_PATH_NODE_VIEW2_HPP
