/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_ATTRIBUTE_HPP
#define KMAP_ATTRIBUTE_HPP

#include "common.hpp"

namespace kmap
{
	class Kmap;
}

namespace kmap::attr {

auto fetch_attr_node( Kmap const& kmap
                    , Uuid const& node )
    -> bool;
auto fetch_attr_owner( Kmap const& km
                     , Uuid const& node )
    -> Result< Uuid >;
auto is_attr( Kmap const& kmap
            , Uuid const& node )
    -> bool;
auto is_in_attr_tree( Kmap const& kmap
                 , Uuid const& node )
    -> bool;
auto is_in_order( Kmap const& kmap
                , Uuid const& parent
                , Uuid const& child )
    -> bool;
// TODO: genesis a component? Add on creation via event outlet?
auto push_genesis( Kmap& kmap
                 , Uuid const& node )
    -> Result< Uuid >;
auto push_order( Kmap& kmap
               , Uuid const& parent
               , Uuid const& child )
    -> Result< Uuid >;
auto pop_order( Kmap& kmap
              , Uuid const& parent
              , Uuid const& child )
    -> Result< Uuid >;

} // namespace kmap::attr

#endif