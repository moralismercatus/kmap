/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TAG_TAG_HPP
#define KMAP_TAG_TAG_HPP

#include "common.hpp"

namespace kmap {

class TagStore
{
public:
};

auto create_tag( Kmap& kmap
               , std::string const& tag_path )
    -> Result< Uuid >;
auto fetch_or_create_tag( Kmap& kmap
                        , std::string const& tag_path )
    -> Result< Uuid >;
auto tag_node( Kmap& kmap
             , Uuid const& target
             , Uuid const& tag )
    -> Result< Uuid >;
auto tag_node( Kmap& kmap
             , Uuid const& target 
             , std::string const& tag_path )
    -> Result< Uuid >;

} // namespace kmap

#endif // KMAP_TAG_TAG_HPP
