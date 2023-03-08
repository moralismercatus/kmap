/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_KMAP_HPP
#define KMAP_KMAP_HPP

#include "common.hpp"
#include "component_store.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <memory>

namespace kmap {

class Kmap
{
    // std::unique_ptr< ComponentStore > component_store_ = {}; // pimpl.
    std::unique_ptr< ComponentStore > component_store_ = {}; // pimpl.
    // TODO: Refactor alias functionality into e.g., AliasStore.
    //       I think that's probably the right call, but then components need to be away of it, so not just "database", but "database" + "alias_store".
    //       A solution may be to make a Network component different from the visjs representation "Network" respresnting "datbase" and "alias_store",
    //       and actually rename that IDK, NetworkVis?
    FsPath database_path_ = {}; // Bit of a hack, so Database::load is able to read the path it's supposed to load. TODO: Better way?

public:
    Kmap();
    Kmap( Kmap const& ) = delete;
    Kmap operator=( Kmap const& ) = delete;

    [[ nodiscard ]]
    auto database_path() const
        -> FsPath const&;
    auto clear()
        -> Result< void >;
    auto clear_component_store()
        -> Result< void >;
    [[ nodiscard ]]
    auto component_store()
        -> ComponentStore&;
    [[ nodiscard ]]
    auto component_store() const
        -> ComponentStore const&;
    template< typename Component >
    auto fetch_component()
    {
        return component_store().fetch_component< Component >();
    }
    template< typename Component >
    auto fetch_component() const
    {
        return component_store().fetch_component< Component >();
    }
    auto has_component_system(){ return component_store_ != nullptr; } // Hack...?
    // TODO: Belongs in com::ResourceStore. Actually unused for now, but can lay dormant there.
        [[ maybe_unused ]]
        auto store_resource( Uuid const& parent
                        , Heading const& heading
                        , std::byte const* data
                        , size_t const& size )
            -> Result< Uuid >;
    // TODO: Non-core utilities should probably not be in Kmap proper. 
    //       Q: Does copy/move not represent core?
    //       A: No, rather update_body() is the core.
    auto load( FsPath const& db_path
             , std::set< std::string > const& components )
        -> Result< void >;
    [[ nodiscard ]]
    auto load( FsPath const& db_path )
        -> Result< void >;
    auto initialize()
        -> Result< void >;
    auto init_component_store()
        -> void;
    auto on_leaving_editor()
        -> Result< void >;

    // TODO: In theory, since this is component-dependent, even this shouldn't be here, lest it is used out of init order.
    [[ nodiscard ]]
    auto root_node_id() const
        -> Uuid const&;
};

class Singleton
{
public:
    static Kmap& instance();

private:
    static std::unique_ptr< Kmap > inst_;
};

} // namespace kmap

#endif // KMAP_KMAP_HPP
