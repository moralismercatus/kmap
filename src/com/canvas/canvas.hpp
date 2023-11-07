/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_CANVAS_HPP
#define KMAP_CANVAS_CANVAS_HPP

#include <com/canvas/common.hpp>
#include <com/event/event_clerk.hpp>
#include <common.hpp>
#include <component.hpp>
#include <js/scoped_code.hpp>
#include <kmap.hpp>
#include <path/node_view2.hpp>

#include <boost/json.hpp>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kmap::com {

/**
 * It should be noted that subdivisions, and their hierarchies, are represented as nodes, so all actions except for "update_*" change the node
 * description, and not the HTML itself. It is for "update" to apply these representations to the HTML.
 * 
 */
class Canvas : public Component
{
    uint32_t next_tabindex_ = 0; // tabIndex enables divs to be focus-able, so give each an increment.
    std::vector< js::ScopedCode > window_events_ = {};
    EventClerk eclerk_;

public:
    static constexpr auto id = "canvas";
    constexpr auto name() const -> std::string_view override { return id; }

    Canvas( Kmap& km
          , std::set< std::string > const& requisites
          , std::string const& description );
    virtual ~Canvas();

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto apply_layout( std::string const& layout )
        -> Result< void >;
    auto apply_layout( boost::json::object const& layout )
        -> Result< void >;
    auto apply_layout( boost::json::object const& layout
                     , Uuid const& parent_pane )
        -> Result< void >;
    [[ nodiscard ]]
    auto complete_path( std::string const& path )
        -> StringVec;
    auto create_html_canvas( Uuid const& id )
        -> Result< void >;
    auto create_html_child_element( std::string const& elem_type
                                  , Uuid const& parent_id
                                  , Uuid const& child_id )
        -> Result< void >;
    auto create_html_element( Uuid const& pane )
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
    auto install_pane( Pane const& pane )
        -> Result< Uuid >;
    auto install_overlay( Overlay const& overlay )
        -> Result< Uuid >;
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
    auto register_standard_events()
        -> Result< void >;
    auto reorient( Uuid const& pane )
        -> Result< void >;
    auto initialize_overlays()
        -> Result< void >;
    auto reset_breadcrumb( Uuid const& supdiv )
        -> Result< Uuid >;
    auto ensure_root_initialized()
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
    auto jump_stack_pane() const
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

} // kmap::com

#endif // KMAP_CANVAS_CANVAS_HPP
