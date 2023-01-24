/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_HPP
#define KMAP_CANVAS_HPP

#include "common.hpp"
#include "component.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace kmap::com {

// If we assert that /misc.window.canvas cannot be renamed nor moved, then it's ID can safely be dynamic, as lookup "/misc.window.canvas" will be reliable.
// TODO: Vary these IDs more. Avoid future conflicts in case somewhere else these hardcoded UUIDs are copied and a single byte changed.
// auto const inline canvas_uuid = Uuid{ 0xfe, 0xae, 0xb8, 0x7a, 0xf2, 0x7f, 0x42, 0x22, 0xa8, 0x00, 0xbb, 0x5c, 0x67, 0xf7, 0xe3, 0x24 };
auto const inline util_canvas_uuid        = Uuid{ 0x5b, 0x00, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 }; // Canvas used exclusively for utility computations (e.g., text measurements).
auto const inline breadcrumb_uuid         = Uuid{ 0x5c, 0x00, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline breadcrumb_table_uuid   = Uuid{ 0x5c, 0x01, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline breadcrumb_fill_uuid    = Uuid{ 0x5c, 0x02, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline cli_uuid                = Uuid{ 0x5d, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline editor_uuid             = Uuid{ 0x5f, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline network_uuid            = Uuid{ 0x5b, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline preview_uuid            = Uuid{ 0x5e, 0xd4, 0x51, 0x26, 0x74, 0xac, 0x4a, 0x5c, 0xb7, 0xf6, 0xa4, 0x86, 0xb2, 0x94, 0x10, 0x33 };
auto const inline text_area_uuid          = Uuid{ 0xee, 0xd3, 0x06, 0xa4, 0x4e, 0xc3, 0x42, 0xc5, 0x8c, 0x89, 0x4d, 0xa5, 0x29, 0xab, 0x1b, 0x70 };
auto const inline workspace_uuid          = Uuid{ 0x94, 0xee, 0xc8, 0x87, 0x46, 0xbb, 0x40, 0xd8, 0xb2, 0x5d, 0xc4, 0xe4, 0x97, 0x03, 0x3c, 0xcf };
auto const inline completion_overlay_uuid = Uuid{ 0xda, 0xe5, 0x12, 0xb0, 0xc1, 0x19, 0x4c, 0xc7, 0xb1, 0x6e, 0xc4, 0xd4, 0x57, 0x53, 0x8f, 0x78 };

enum class Orientation
{
    horizontal
,   vertical
};

auto to_string( Orientation const& orientation )
    -> std::string;

struct Division
{
    Orientation orientation = {};
    float base = {}; // TODO: Use constrained numeric type such that [0,100] is enforced.
    bool hidden = false;
    std::string elem_type = {};
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
auto fetch_overlay_root( Kmap& kmap )
    -> Result< Uuid >;
auto fetch_overlay_root( Kmap const& kmap )
    -> Result< Uuid >;
[[ nodiscard ]]
auto fetch_subdiv_root( Kmap& kmap )
    -> Result< Uuid >;

/**
 * It should be noted that subdivisions, and their hierarchies, are represented as nodes, so all actions except for "update_*" change the node
 * description, and not the HTML itself. It is for "update" to apply these representations to the HTML.
 * 
 */
class Canvas : public Component
{
    uint32_t next_tabindex_ = 0; // tabIndex enables divs to be focus-able, so give each an increment.
    std::vector< Uuid > canvas_element_stack_ = {};
    std::vector< js::ScopedCode > window_events_ = {};

public:
    static constexpr auto id = "canvas";
    constexpr auto name() const -> std::string_view override { return id; }

    Canvas( Kmap& kmap
          , std::set< std::string > const& requisites
          , std::string const& description );
    virtual ~Canvas();

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    [[ nodiscard ]]
    auto complete_path( std::string const& path )
        -> StringVec;
    auto create_html_canvas( Uuid const& id )
        -> Result< void >;
    auto create_html_child_element( std::string const& elem_type
                                  , Uuid const& parent_id
                                  , Uuid const& child_id )
        -> Result< void >;
    auto create_html_root_element( Uuid const& root_pane )
        -> Result< void >;
    auto create_overlay( Uuid const& id
                       , std::string const& heading )
        -> Result< Uuid >;
    auto create_subdivision( Uuid const& parent
                           , Uuid const& child
                           , std::string const& heading
                           , Division const& subdiv )
        -> Result< Uuid >;
    auto create_subdivision( Uuid const& parent
                           , std::string const& heading
                           , Division const& subdiv )
        -> Result< Uuid >;
    auto delete_pane( Uuid const& pane )
        -> Result< void >;
    auto delete_subdivisions( Uuid const& pane )
        -> Result< void >;
    [[ nodiscard ]]
    auto dimensions( Uuid const& target )
        -> Result< Dimensions >;
    [[ nodiscard ]]
    auto expand_path( std::string const& path )
        -> std::string;
    auto fetch_canvas_root()
        -> Result< Uuid >;
    auto fetch_canvas_root() const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_pane( std::string const& path )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_parent_pane( Uuid const& node )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_subdivisions( Uuid const& pane )
        -> Result< UuidSet >;
    auto focus( Uuid const& pane )
        -> Result< void >;
    [[ nodiscard ]]
    auto height() const
        -> uint32_t;
    auto hide( Uuid const& pane )
        -> Result< void >;
    // auto init_event_callbacks()
    //     -> Result< void >;
    auto install_events()
        -> Result< void >;
    auto install_options()
        -> Result< void >;
    auto make_subdivision( Uuid const& target
                         , Division const& subdiv )
        -> Result< void >;
    [[ nodiscard ]]
    auto orient( Uuid const& pane
               , Orientation const& orientation )
        -> Result< void >;
    [[ nodiscard ]]
    auto is_overlay( Uuid const& node )
        -> bool;
    [[ nodiscard ]]
    auto is_pane( Uuid const& node ) const
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
    auto redraw()
        -> Result< void >;
    auto reorient( Uuid const& pane )
        -> Result< void >;
    auto initialize_panes()
        -> Result< void >;
    auto initialize_overlays()
        -> Result< void >;
    auto reset_breadcrumb( Uuid const& supdiv )
        -> Result< Uuid >;
    auto initialize_root()
        -> Result< Uuid >;
    auto reveal( Uuid const& pane )
        -> Result< void >;
    auto set_breadcrumb( UuidVec const& bc )
        -> Result< void >;
    auto subdivide( Uuid const& parent_pane
                  , Uuid const& pane_id
                  , Heading const& heading
                  , Division const& subdiv )
        -> Result< Uuid >;
    auto subdivide( Uuid const& parent_pane
                  , Heading const& heading
                  , Division const& subdiv )
        -> Result< Uuid >;
    auto update_overlays()
        -> Result< void >;
    auto update_all_panes()
        -> Result< void >;
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
    auto breadcrumb_fill_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto breadcrumb_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto breadcrumb_table_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto canvas_pane() const
        -> Uuid;
    [[ nodiscard ]]
    auto completion_overlay() const
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
};

// exists( std::string_view const div ) 
// operator[]( std::string_view const div ) fetch division
// begin/end for iterating through all subdivisions. Just return begin/end to map< path, Division >

} // namespace kmap::com

namespace kmap
{
    template<>
    auto from_string( std::string const& s )
        -> Result< com::Orientation >;
}

#endif // KMAP_CANVAS_HPP
