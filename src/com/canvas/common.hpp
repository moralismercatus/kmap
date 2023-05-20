/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_COMMON_HPP
#define KMAP_CANVAS_COMMON_HPP

#include <common.hpp>
#include <component.hpp>
#include <kmap.hpp>
#include <path/node_view2.hpp>

#include <boost/json.hpp>

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
auto const inline jump_stack_uuid         = Uuid{ 0xba, 0xe6, 0x12, 0xb0, 0xc1, 0x19, 0x4c, 0xc7, 0xb1, 0x6e, 0xc4, 0xd4, 0x57, 0x53, 0x8f, 0x78 };

auto const inline min_pane_size_multiplier = 0.01f;

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

struct Pane
{
    Uuid id = {};
    std::string heading = {};
    Division division = {};
};

struct Overlay
{
    Uuid id = {};
    std::string heading = {};
    bool hidden = true;
    std::string elem_type = {};
};

auto operator<<( std::ostream& lhs 
               , Dimensions const& rhs )
    -> std::ostream&;

// exists( std::string_view const div ) 
// operator[]( std::string_view const div ) fetch division
// begin/end for iterating through all subdivisions. Just return begin/end to map< path, Division >

auto fetch_layout( FsPath const& path )
    -> Result< boost::json::object >;
auto fetch_pane( boost::json::object const& layout )
    -> Result< Pane >;
auto fetch_pane( boost::json::object const& layout
               , Uuid const& id )
    -> Result< Pane >;
auto fetch_parent_pane( Kmap const& km
                      , boost::json::object const& layout
                      , Uuid const& root_pane
                      , Uuid const& child_pane )
    -> Result< Uuid >;
auto fetch_subdivisions( boost::json::object const& layout )
    -> std::vector< boost::json::object >;
auto has_pane_id( boost::json::object const& layout
                , Uuid const& id )
    -> bool;
auto has_subdivision( boost::json::object const& layout
                    , Uuid const& id )
    -> bool;
auto load_initial_layout( FsPath const& path )
    -> Result< std::string >;

} // namespace kmap::com

namespace kmap::view2::canvas
{
    auto const window_root = anchor::abs_root
                           | view2::direct_desc( "meta.setting.window" );
    auto const canvas_root = window_root
                           | view2::child( "canvas" );
    auto const overlay_root = canvas_root
                            | view2::child( "overlay" );
    // TODO: Needs chaining of predicates ability, or maybe just view2::node in this case.
    // auto const pane = view2::node( view2::child( "orientation" )
    //                              | view2::child( "base" )
    //                              | view2::child( "subdivision" ) );
    // Usage: anchor::node( curr_pane ) | view2::canvas::parent_pane | act2::fetch_node( km );
    // auto const parent_pane = view2::parent( "subdivision" )
    //                        | view2::parent( view2::canvas::pane );
    
}

namespace kmap
{
    template<>
    auto from_string( std::string const& s )
        -> Result< com::Orientation >;
}

#endif // KMAP_CANVAS_COMMON_HPP
