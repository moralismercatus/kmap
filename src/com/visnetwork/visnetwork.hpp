/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_VISUAL_NETWORK_HPP
#define KMAP_VISUAL_NETWORK_HPP

#include "com/canvas/pane_clerk.hpp"
#include "com/event/event_clerk.hpp"
#include "com/option/option_clerk.hpp"
#include "common.hpp"
#include "component.hpp"
#include "utility.hpp"
#include <js_iface.hpp>

#include <ostream>
#include <string>
#include <tuple>
#include <memory>

namespace emscripten
{
    class val;
}

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

struct Position2D
{
    int32_t x = {};
    int32_t y = {};
};

auto operator<<( std::ostream&, Position2D const& )
    -> std::ostream&;

/**
 * @note: It looks clean to store the JS Network instance as a member here, but the trouble is that emscripten lacks support for JS exception => C++, so any JS exception
 *        raised while execiting `js_new_->call()`s will result in an uncaught (read: cryptic) error lacking details. Is there a way aroud this?
 *        Update: As a matter of fact, emscripten does support JS, C++ exception interoperability, or ways to achieve it. I think maybe the situation here is
 * //             in the js_nw_->action style operations doesn't communicate exceptions to C++.
 */
class VisualNetwork : public Component
{
    OptionClerk oclerk_;
    EventClerk eclerk_;
    PaneClerk pclerk_;
    std::shared_ptr< emscripten::val > js_nw_; // TODO: Use unique_ptr. Again, some reason destructor and fwd decl don't seem to work with unique_ptr.
    std::vector< js::ScopedCode > js_event_handlers_ = {};

public:
    static constexpr auto id = "visnetwork";
    constexpr auto name() const -> std::string_view override { return id; }

    VisualNetwork( Kmap& km
                 , std::set< std::string > const& requisites
                 , std::string const& description );
    VisualNetwork( VisualNetwork const& ) = delete;
    VisualNetwork( VisualNetwork&& ) = default;
    virtual ~VisualNetwork();

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    // Core.
    auto add_edge( Uuid const& from
                 , Uuid const& to )
        -> Result< void >;
    auto apply_static_options()
        -> Result< void >;
    auto center_viewport_node( Uuid const& id )
        -> void;
    [[ nodiscard ]]
    auto children( Uuid const& parent ) const
        -> std::vector< Uuid >;
    auto color_node( Uuid const& id
                   , Color const& color )
        -> void;
    auto color_node( Uuid const& id )
        -> void;
    [[ maybe_unused ]]
    auto create_node( Uuid const& id
                    , Title const& label )
        -> Result< void >;
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
    auto get_appropriate_node_font_face( Uuid const& id ) const // TODO: Should be from option?
        -> std::string;
    [[ nodiscard ]]
    auto nodes() const
        -> std::vector< Uuid >;
    [[ nodiscard ]]
    auto position( Uuid const& id ) const
        -> Position2D;
    auto erase_node( Uuid const& id )
        -> Result< void >;
    auto register_panes()
        -> Result< void >;
    auto register_standard_events()
        -> Result< void >;
    auto register_standard_options()
        -> Result< void >;
    auto remove_nodes()
        -> void;
    auto remove_edge( Uuid const& from
                    , Uuid const& to )
        -> Result< void >;
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
        -> Result< void >;
    auto viewport_scale() const
        -> float;

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
    auto scale_viewport( float const scale )
        -> Result< void >;
    auto to_titles( UuidPath const& path ) const
        -> StringVec;

    // For testing purposes. Access to raw JS object.
    auto underlying_js_network()
        -> std::shared_ptr< emscripten::val >;

protected:
    auto install_events()
        -> Result< void >;
};

[[nodiscard]]
auto format_node_label( Kmap const& km
                      , Uuid const& node )
    -> std::string;

} // namespace kmap::com

#endif // KMAP_VISUAL_NETWORK_HPP
