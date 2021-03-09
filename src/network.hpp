/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_NETWORK_HPP
#define KMAP_NETWORK_HPP

#include "common.hpp"
#include "utility.hpp"

#include <ostream>
#include <string>
#include <tuple>
#include <memory>

namespace emscripten
{
    class val;
}

namespace kmap {

struct Position2D
{
    int32_t x = {};
    int32_t y = {};
};

auto operator<<( std::ostream&, Position2D const& )
    -> std::ostream&;

class Network
{
public:
    Network( Uuid const& container );
    Network( Network const& ) = delete;
    Network( Network&& ) = default;
    ~Network();

    // Core.
    auto add_edge( Uuid const& from
                 , Uuid const& to )
        -> void;
    auto center_viewport_node( Uuid const& id )
        -> void;
    [[ nodiscard ]]
    auto children( Uuid const& parent ) const
        -> std::vector< Uuid >;
    [[ maybe_unused ]]
    auto create_node( Uuid const& id
                    , Title const& title )
        -> Uuid;
    auto create_nodes( std::vector< Uuid > const& ids 
                     , std::vector< Title > const& titles )
        -> void; // TODO: Should return bool.
    auto create_edges( std::vector< std::pair< Uuid, Uuid > > const& edges )
        -> void; // TODO: Should return bool.
    [[ nodiscard ]]
    auto edge_exists( Uuid const& p1
                    , Uuid const& p2 ) const
        -> bool;
    [[ nodiscard ]]
    auto exists( Uuid const& id ) const
        -> bool;
    [[ nodiscard ]]
    auto fetch_parent( Uuid const& id ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_position( Uuid const& id ) const
        -> Result< Position2D >;
    auto focus()
        -> void;
    [[ nodiscard ]]
    auto nodes() const
        -> std::vector< Uuid >;
    [[ nodiscard ]]
    auto position( Uuid const& id ) const
        -> Position2D;
    auto remove( Uuid const& id )
        -> void;
    auto remove_nodes()
        -> void;
    auto remove_edge( Uuid const& from
                    , Uuid const& to )
        -> void;
    auto remove_edges()
        -> void;
    [[ maybe_unused ]]
    auto select_node( Uuid const& id )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto selected_node() const
        -> Uuid;
    auto update_title( Uuid const& id
                     , Title const& title )
        -> void;

    // Convenience.
    auto child_titles( Uuid const& parent ) const
        -> std::vector< Title >;
    auto color_node_border( Uuid const& id
                          , Color const& color )
        -> void;
    auto color_node_background( Uuid const& id
                              , Color const& color )
        -> void;
    auto change_node_font( Uuid const& id
                         , std::string const& face
                         , Color const& color )
        -> Result< void >;
    auto exists( Title const& title ) const
        -> bool;
    auto fetch_child( Uuid const& parent
                    , Title const& title ) const
        -> Result< Uuid >;
    auto fetch_title( Uuid const& id ) const
        -> Result< Title >;
    auto title( Uuid const& id ) const
        -> Title;
    auto is_child( Uuid parent
                 , Uuid const& child ) const
        -> bool;
    auto is_child( Uuid parent
                 , Title const& child ) const
        -> bool;
    auto is_child( std::vector< Uuid > const& parents
                 , Uuid const& child ) const
        -> bool;
    auto siblings_exclusive( Uuid const& id ) const
        -> std::vector< Uuid >;
    auto siblings_inclusive( Uuid const& id ) const
        -> std::tuple< std::vector< Uuid >
                     , uint32_t >;
    auto to_titles( UuidPath const& path ) const
        -> StringVec;

protected:
private:
    std::shared_ptr< emscripten::val > js_nw_; // TODO: Use unique_ptr. Again, some reason destructor and fwd decl don't seem to work with unique_ptr.
};

} // namespace kmap

#endif // KMAP_NETWORK_HPP
