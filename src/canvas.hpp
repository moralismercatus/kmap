/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_HPP
#define KMAP_CANVAS_HPP

#include "common.hpp"
#include "kmap.hpp"

#include <map>
#include <memory>
#include <string_view>

namespace kmap {

// If we assert that /misc.window.canvas cannot be renamed nor moved, then it's ID can safely be dynamic, as lookup "/misc.window.canvas" will be reliable.
// TODO: Vary these IDs more. Avoid future conflicts in case somewhere else these hardcoded UUIDs are copied and a single byte changed.
// auto const inline canvas_uuid = Uuid{ 0xfe, 0xae, 0xb8, 0x7a, 0xf2, 0x7f, 0x42, 0x22, 0xa8, 0x00, 0xbb, 0x5c, 0x67, 0xf7, 0xe3, 0x24 };
auto const inline breadcrumb_uuid   = Uuid{ 0x5c, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline cli_uuid   = Uuid{ 0x5d, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline editor_uuid   = Uuid{ 0x5f, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline network_uuid   = Uuid{ 0x5b, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline preview_uuid   = Uuid{ 0x5e, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline text_area_uuid = Uuid{ 0xee, 0xd3, 0x06, 0xa4, 0x4e, 0xc3, 0x42, 0xc5, 0x8c, 0x89, 0x4d, 0xa5, 0x29, 0xab, 0x1b, 0x70 };
auto const inline workspace_uuid = Uuid{ 0x94, 0xee, 0xc8, 0x87, 0x46, 0xbb, 0x40, 0xd8, 0xb2, 0x5d, 0xc4, 0xe4, 0x97, 0x03, 0x3c, 0xcf };

auto set_fwd_breadcrumb( std::string_view const text )
    -> bool;

enum class Orientation
{
    horizontal
,   vertical
};

auto to_string( Orientation const& orientation )
    -> std::string;
template<>
auto from_string( std::string const& s )
    -> Result< Orientation >;

struct Division
{
    Orientation orientation = {};
    float base = {}; // TODO: Use constrained numeric type such that [0,100] is enforced.
    bool hidden = false;
    std::map< std::string, std::string > style_map = {};
};

struct Dimensions
{
    uint32_t top = {};
    uint32_t bottom = {};
    uint32_t left = {};
    uint32_t right = {};
};

auto operator<<( std::ostream& lhs 
               , Dimensions const& rhs )
    -> std::ostream&;

[[ nodiscard ]]
auto fetch_canvas_root( Kmap& kmap )
    -> Result< Uuid >;
[[ nodiscard ]]
auto fetch_subdiv_root( Kmap& kmap )
    -> Result< Uuid >;

class Canvas
{
public:
    Canvas( Kmap& kmap );

    [[ nodiscard ]]
    auto complete_path( std::string const& path )
        -> StringVec;
    auto create_subdivision( Uuid const& parent
                           , Uuid const& child
                           , std::string const& heading
                           , Division const& subdiv
                           , std::string const& elem_type )
        -> Result< Uuid >;
    auto create_subdivision( Uuid const& parent
                           , std::string const& heading
                           , Division const& subdiv
                           , std::string const& elem_type )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto dimensions( Uuid const& target )
        -> Result< Dimensions >;
    [[ nodiscard ]]
    auto expand_path( std::string const& path )
        -> std::string;
    [[ nodiscard ]]
    auto fetch_pane( std::string const& path )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_parent_pane( Uuid const& node )
        -> Result< Uuid >;
    auto focus( Uuid const& pane )
        -> Result< void >;
    [[ nodiscard ]]
    auto height() const
        -> uint32_t;
    auto hide( Uuid const& pane )
        -> Result< void >;
    auto make_subdivision( Uuid const& target
                         , Division const& subdiv )
        -> Result< void >;
    [[ nodiscard ]]
    auto orient( Uuid const& pane
               , Orientation const& orientation )
        -> Result< void >;
    [[ nodiscard ]]
    auto is_pane( Uuid const& node )
        -> bool;
    [[ nodiscard ]]
    auto pane_base( Uuid const& pane )
        -> Result< float >;
    [[ nodiscard ]]
    auto pane_orientation( Uuid const& pane )
        -> Result< Orientation >;
    [[ nodiscard ]]
    auto pane_hidden( Uuid const& pane )
        -> Result< bool >;
    auto pane_subdivision( Uuid const& pane )
        -> Result< Uuid >;
    auto pane_path( Uuid const& pane )
        -> Result< std::string >;
    auto rebase( Uuid const& pane
               , float const base )
        -> Result< void >;
    auto reorient( Uuid const& pane )
        -> Result< void >;
    auto reset()
        -> Result< void >;
    auto reset_root()
        -> Result< Uuid >;
    auto reveal( Uuid const& pane )
        -> Result< void >;
    auto subdivide( Uuid const& parent_pane
                  , Uuid const& pane_id
                  , Heading const& heading
                  , Division const& subdiv
                  , std::string const& elem_type )
        -> Result< Uuid >;
    auto subdivide( Uuid const& parent_pane
                  , Heading const& heading
                  , Division const& subdiv
                  , std::string const& elem_type )
        -> Result< Uuid >;
    auto update_pane( Uuid const& pane )
        -> Result< void >;
    auto update_pane_descending( Uuid const& root ) // TODO: Lineal< window_root, pane >
        -> Result< void >;
    auto update_panes()
        -> Result< void >;
    [[ nodiscard ]]
    auto width() const
        -> uint32_t;

    [[ nodiscard ]]
    auto breadcrumb_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto cli_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto editor_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto network_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto preview_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto text_area_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto workspace_pane() const
        -> Uuid;

protected:
    auto hide_internal( Uuid const& pane
                      , bool const hidden )
        -> Result< void >;
    auto rebase_internal( Uuid const& pane
               , float const base )
        -> Result< void >;
    auto reorient_internal( Uuid const& pane
                 , Orientation const& orientation )
        -> Result< void >;

private:
    Kmap& kmap_;
};

// exists( std::string_view const div ) 
// operator[]( std::string_view const div ) fetch division
// begin/end for iterating through all subdivisions. Just return begin/end to map< path, Division >

// TODO:
// https://www.w3schools.com/jsref/event_onresize.asp - use callback for "onresize" to recalculate Divisions when window changes size.

} // namespace kmap

#endif // KMAP_CANVAS_HPP
