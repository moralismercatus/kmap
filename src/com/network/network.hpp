/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_NETWORK_HPP
#define KMAP_NETWORK_HPP

#include "com/event/event_clerk.hpp"
#include "com/option/option_clerk.hpp"
#include "com/network/alias.hpp"
#include "common.hpp"
#include "component.hpp"
#include "utility.hpp"

#include <ostream>
#include <string>
#include <tuple>
#include <memory>

namespace kmap::com {

/**
 * @note: It looks clean to store the JS Network instance as a member here, but the trouble is that emscripten lacks support for JS exception => C++, so any JS exception
 *        raised while execiting `js_new_->call()`s will result in an uncaught (read: cryptic) error lacking details. Is there a way aroud this?
 *        Update: As a matter of fact, emscripten does support JS, C++ exception interoperability, or ways to achieve it. I think maybe the situation here is
 * //             in the js_nw_->action style operations doesn't communicate exceptions to C++.
 */
class Network : public Component//< Database >
{
    AliasStore astore_;
    // TODO: Q: Should selected_node_ be a thing? Or does the notion of a selected node only make sense in reference to a visual?
    //       A: I think it makes sense outside the context of a visual. For example, commands operate on the assumption that there's a "current_node".
    //          A visual could be used to change the current node, but the current node shouldn't be dependent on the visual.
    Uuid selected_node_ = Uuid{ 0 };

public:
    static constexpr auto id = "network";
    constexpr auto name() const -> std::string_view override { return id; }

    Network( Kmap& km
           , std::set< std::string > const& requisites
           , std::string const& description );
    Network( Network const& ) = delete;
    Network( Network&& ) = default;
    virtual ~Network() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    // Core
    auto alias_store()
        -> AliasStore&;
    auto alias_store() const
        -> AliasStore const&;
    auto create_alias( Uuid const& src
                     , Uuid const& dst )
        -> Result< Uuid >;
    auto create_child( Uuid const& parent
                     , Uuid const& child
                     , Heading const& heading
                     , Title const& title )
        -> Result< Uuid >;
    auto create_child( Uuid const& parent
                     , Uuid const& child
                     , Heading const& heading )
        -> Result< Uuid >;
    auto create_child( Uuid const& parent // TODO: This must ensure "heading" is a valid heading. If I make a Heading type that cannot be invalid, the problem solves itself.
                     , Heading const& heading )
        -> Result< Uuid >;
    auto create_child( Uuid const& parent
                     , Heading const& heading
                     , Title const& title )
        -> Result< Uuid >;
    auto erase_node( Uuid const& id )
        -> Result< Uuid >;
    auto erase_attr( Uuid const& id )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto exists( Uuid const& node ) const
        -> bool;
    auto fetch_aliased_ancestry( Uuid const& id ) const // TODO: Replace with `view::make( id ) | view::ancestor( view::alias ) | view::to_node_set()`, or equivalent.
        -> std::vector< Uuid >;
    auto fetch_attr_node( Uuid const& id ) const
        -> Result< Uuid >;
    auto fetch_body( Uuid const& id ) const
        -> Result< std::string >;
    [[ nodiscard ]]
    auto fetch_children( Uuid const& parent ) const
        -> kmap::UuidSet;
    auto fetch_heading( Uuid const& id ) const
        -> Result< Heading >;
    // TODO [cleanup]: move to attr.hpp? Rather, node.genesis component?
        [[ nodiscard ]]
        auto fetch_genesis_time( Uuid const& id ) const
            -> Optional< uint64_t >; // TODO: Result< uint64_t >
    auto fetch_nodes( Heading const& heading ) const
        -> UuidSet;
    auto fetch_ordering_position( Uuid const& node ) const
        -> Result< uint32_t >;
    auto fetch_parent( Uuid const& child ) const
        -> Result< Uuid >;
    auto resolve( Uuid const& node ) const
        -> Uuid;
    auto select_node( Uuid const& id )
        -> Result< Uuid >;
    auto select_node_initial( Uuid const& node )
        -> Result< void >;
    [[ nodiscard ]]
    auto selected_node() const
        -> Uuid;
    auto set_ordering_position( Uuid const& id
                              , uint32_t pos )
        -> Result< void >;
    auto update_alias( Uuid const& from
                     , Uuid const& to )
        -> Result< Uuid >;
    auto update_aliases( Uuid const& node )
        -> Result< void >;
    auto update_body( Uuid const& id
                    , std::string const& contents )
        -> Result< void >;
    auto update_heading( Uuid const& id
                       , Heading const& heading )
        -> Result< void >;
    auto update_title( Uuid const& id
                     , Title const& title )
        -> Result< void >;
    
    // Util
    [[ nodiscard ]]
    auto adjust_selected( Uuid const& root ) const // TODO: Named clearly (sarcasm). So, what does it do?
        -> Uuid;
    // TODO [cleanup]: Belongs in node_view: view::make( n1 ) | view::parent | view::child( view::all_of( n1, n2 ) ) | act::exists( km );
    //                 Because this routine may be quite common, perhaps a path.hpp/utility.hpp entry is warranted.
        [[ nodiscard ]]
        auto are_siblings( Uuid const& n1
                         , Uuid const& n2 ) const
            -> bool;
    auto copy_body( Uuid const& src
                , Uuid const& dst )
        -> Result< void >;
    // TODO: Probably safe to move this to com::Network without qualification. Strictly speaking, it's a convenience, as its value can be easily determined via fetch_parent;
    //       with that in mind, perhaps it belongs as a free function: distance( nw, anc, desc );
    //       I don't see a good way of using node_view in this case. Maybe...: view::make( anc ) | view::desc( desc ) | act::distance( nw ), I suppose?
    //       I suppose it would actually work, and maybe well, just so long as it resolves to_node_set( km ).size() == 1.
    //       It'd work! 'tis great! Do it! Similar to abs_path....
    [[ nodiscard ]]
    auto distance( Uuid const& ancestor
                 , Uuid const& descendant ) const
        -> uint32_t;
    [[ nodiscard ]] // TODO: deprecate... replace with view::exists, but what about selection-relative paths?
    auto exists( Heading const& heading ) const
        -> bool;
    template< typename... Args >
    // TODO: Non-core, convenience, so probably best exists( nw, n1, n2, n3... )
    [[ nodiscard ]]
    auto exist( Args const&... args ) const -> bool { return ( exists( args ) && ... ); } // Warning: arguments must be explicit for template deduction, so e.g., "/" must be Heading{ "/" }.
    auto fetch_above( Uuid const& id ) const
        -> Result< Uuid >;
    auto fetch_below( Uuid const& id ) const
        -> Result< Uuid >;
    auto fetch_child( Uuid const& parent 
                    , Heading const& heading ) const
        -> Result< Uuid >;
    auto fetch_next_selected_as_if_erased( Uuid const& node ) const
        -> Result< Uuid >;
    auto fetch_title( Uuid const& node ) const
        -> Result< Title >;
    auto fetch_visible_nodes_from( Uuid const& node ) const
        -> std::vector< Uuid >;
    // TODO [cleanup]: Can be replaced by node_view.
        template< typename... Nodes >
        [[ nodiscard ]]
        auto is_child( Uuid const& parent
                     , Nodes const&... children ) const
            -> bool
        {
            return ( ... && is_child_internal( parent, children ) );
        }
    // TODO [cleanup]: Can be replaced by node_view.
        [[ nodiscard ]]
        auto is_lineal( Uuid const& ancestor
                    , Uuid const& descendant ) const
            -> bool;
        [[ nodiscard ]]
        auto is_lineal( Uuid const& ancestor
                    , Heading const& descendant ) const
            -> bool;
    [[ nodiscard ]]
    auto is_root( Uuid const& node ) const
        -> bool;
    [[ nodiscard ]]
    auto is_top_alias( Uuid const& id ) const
        -> bool;
    auto load_aliases()
        -> Result< void >;
    auto move_body( Uuid const& src
                  , Uuid const& dst )
        -> Result< void >;
    auto move_node( Uuid const& from
                  , Uuid const& to )
        -> Result< Uuid >;
    auto reorder_children( Uuid const& parent
                         , std::vector< Uuid > const& children )
        -> Result< void >;
    [[ nodiscard ]]
    auto root_node()
        -> Uuid;
    [[ maybe_unused ]]
    auto swap_nodes( Uuid const& t1
                    , Uuid const& t2 )
        -> Result< std::pair< Uuid, Uuid > >;

    // TODO: Where do these travel functions go? They are not actually core functionality, as they are effectively fetch_children, fetch_ordering, and select_node.
    [[ maybe_unused ]]
    auto travel_bottom() // TODO: Add unit test for this.
        -> Uuid;
    auto travel_down()
        -> Result< Uuid >;
    auto travel_left()
        -> Result< Uuid >;
    auto travel_right()
        -> Result< Uuid >;
    auto travel_top() // TODO: Add unit test for this.
        -> Uuid;
    auto travel_up()
        -> Result< Uuid >;

protected:
    auto create_internal_alias( Uuid const& src
                              , Uuid const& dst )
        -> Result< Uuid >;
    auto create_child_internal( Uuid const& parent
                              , Uuid const& child
                              , Heading const& heading
                              , Title const& title )
        -> kmap::Result< void >;
    auto erase_node_internal( Uuid const& id )
        -> Result< void >;
    auto erase_node_leaf( Uuid const& id )
        -> Result< void >;
    [[ nodiscard ]]
    auto is_child_internal( Uuid const& parent
                          , Uuid const& id ) const
        -> bool;
    [[ nodiscard ]]
    auto is_child_internal( Uuid const& parent
                          , Heading const& id ) const
        -> bool;
};

} // namespace kmap::com

#endif // KMAP_NETWORK_HPP
