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
// TODO: Replace fetch_tag_root with node_view equivalents: view::tag_root, to allow for create, fetch, fetch or create, to_node_set variants.
auto fetch_tag_root( Kmap& kmap )
    -> Result< Uuid >;
auto fetch_tag_root( Kmap const& kmap )
    -> Result< Uuid >;
auto fetch_tag( Kmap const& kmap
              , std::string const& tag_path )
    -> Result< Uuid >;
auto fetch_or_create_tag( Kmap& kmap
                        , std::string const& tag_path )
    -> Result< Uuid >;
auto has_tag( Kmap const& kmap
            , Uuid const& node
            , Uuid const& tag )
    -> bool;
auto has_tag( Kmap const& kmap
            , Uuid const& node
            , std::string const& tag_path )
    -> bool;
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
