/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TAG_TAG_HPP
#define KMAP_TAG_TAG_HPP

#include "com/cmd/cclerk.hpp"
#include "common.hpp"
#include "component.hpp"

#include <set>
#include <string>

namespace kmap::com {

class TagStore : public Component
{
    CommandClerk cclerk_;

public:
    static constexpr auto id = "tag_store";
    constexpr auto name() const -> std::string_view override { return id; }

    TagStore( Kmap& kmap
            , std::set< std::string > const& requisites
            , std::string const& description );
    virtual ~TagStore() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto create_tag( std::string const& tag_path )
        -> Result< Uuid >;
    // TODO: Replace fetch_tag_root with node_view equivalents: view::tag_root, to allow for create, fetch, fetch or create, to_node_set variants.
    auto fetch_tag_root() const
        -> Result< Uuid >;
    auto fetch_tag_root()
        -> Result< Uuid >;
    auto fetch_tag( std::string const& tag_path ) const
        -> Result< Uuid >;
    auto fetch_or_create_tag( std::string const& tag_path )
        -> Result< Uuid >;
    auto has_tag( Uuid const& node
                , Uuid const& tag ) const
        -> bool;
    auto has_tag( Uuid const& node
                , std::string const& tag_path ) const
        -> bool;
    auto install_standard_commands()
        -> Result< void >;
    auto tag_node( Uuid const& target
                 , Uuid const& tag )
        -> Result< Uuid >;
    auto tag_node( Uuid const& target 
                 , std::string const& tag_path )
        -> Result< Uuid >;
    };


} // namespace kmap::com

#endif // KMAP_TAG_TAG_HPP
