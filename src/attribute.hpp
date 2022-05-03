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

auto is_in_attr_tree( Kmap const& kmap
                    , Uuid const& node )
    -> bool;
auto is_in_order( Kmap const& kmap
                , Uuid const& parent
                , Uuid const& child )
    -> bool;
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