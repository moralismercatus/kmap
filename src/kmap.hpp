/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_KMAP_HPP
#define KMAP_KMAP_HPP

#include "alias.hpp"
#include "common.hpp"
#include "jump_stack.hpp"
#include "node_fetcher.hpp"
#include "path/node_view.hpp"
#include "text_area.hpp"
#include "utility.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <regex>

namespace kmap {

class Canvas;
class Cli;
class Database;
class Environment;
class EventStore;
class Network;
class OptionStore;

namespace chrono
{
    class Timer;
}

namespace db
{
    class Autosave;
}

class Kmap
{
public:
    Kmap();
    Kmap( Kmap const& ) = delete;
    Kmap operator=( Kmap const& ) = delete;

    [[ nodiscard ]]
    auto autosave()
        -> db::Autosave&;
    [[ nodiscard ]]
    auto autosave() const
        -> db::Autosave const&;
    auto parse_cli( std::string const& input )
        -> void; // Return simple void to simplify EMCC binding.
    [[ nodiscard ]]
    auto instance()
        -> Environment&;
    [[ nodiscard ]]
    auto instance() const
        -> Environment const&;
    [[ nodiscard ]]
    auto database()
        -> Database&;
    [[ nodiscard ]]
    auto database() const
        -> Database const&;
    [[ nodiscard ]]
    auto network()
        -> Network&;
    [[ nodiscard ]]
    auto network() const
        -> Network const&;
    [[ nodiscard ]]
    auto aliases()
        -> AliasSet&;
    [[ nodiscard ]]
    auto aliases() const
        -> AliasSet const&;
    [[ nodiscard ]]
    auto canvas()
        -> Canvas&;
    [[ nodiscard ]]
    auto canvas() const
        -> Canvas const&;
    [[ nodiscard ]]
    auto cli()
        -> Cli&;
    [[ nodiscard ]]
    auto cli() const
        -> Cli const&;
    [[ nodiscard ]]
    auto text_area()
        -> TextArea&;
    [[ nodiscard ]]
    auto text_area() const
        -> TextArea const&;
    [[ nodiscard ]]
    auto root_node_id() const
        -> Uuid const&;
    [[ nodiscard ]]
    auto event_store()
        -> EventStore&;
    [[ nodiscard ]]
    auto option_store()
        -> OptionStore&;
    [[ nodiscard ]]
    auto option_store() const
        -> OptionStore const&;
    // TODO [cleanup]: All these abs_path varieties are composables. They are better served with pipes than functions for each composition.
    [[ nodiscard ]]
    auto absolute_path_uuid( Lineal const& node ) const
        -> UuidPath;
    [[ nodiscard ]]
    auto absolute_path_uuid( Uuid const& node ) const // view::make( node ) | view::absolute_path( kmap ) | view::to_single
        -> UuidPath;
    [[ nodiscard ]]
    auto absolute_path( Uuid const& root
                      , Uuid const& id ) const // view::make( node ) | view::absolute_path( kmap ) | view::to_single | view::to_path
        -> HeadingPath;
    [[ nodiscard ]]
    auto absolute_path( Uuid const& id ) const
        -> HeadingPath;
    [[ nodiscard ]]
    auto absolute_ipath( Uuid const& id ) const
        -> HeadingPath;
    [[ nodiscard ]]
    auto absolute_path_flat( Uuid const& id ) const
        -> Heading;
    [[ nodiscard ]]
    auto absolute_ipath_flat( Uuid const& id ) const
        -> InvertedHeading;
    [[ nodiscard ]]
    auto adjust_selected( Uuid const& root ) const
        -> Uuid;
    [[ nodiscard ]]
    auto fetch_above( Uuid const& id ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_below( Uuid const& id ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_aliases_to( Uuid const& id ) const
        -> std::vector< Uuid >; // TODO: Probably should be UuidSet, to show uniqueness.
    [[ nodiscard ]]
    auto fetch_nearest_ancestor( Uuid const& root
                               , Uuid const& leaf
                               , std::regex const& geometry ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_next_selected_as_if_erased( Uuid const& node ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_visible_nodes_from( Uuid const& id ) const
        -> std::vector< Uuid >;
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Uuid const& child
                     , Heading const& heading
                     , Title const& title )
        -> kmap::Result< Uuid >;
    // TODO: Make all create_child signatures uniform: parent, [child,] heading across Kmap/Network/Database.
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Uuid const& child
                     , Heading const& heading )
        -> kmap::Result< Uuid >;
    // TODO: This must ensure "heading" is a valid heading. If I make a Heading type that cannot be invalid, the problem solves itself.
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Heading const& heading )
        -> kmap::Result< Uuid >;
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Heading const& heading
                     , Title const& title )
        -> kmap::Result< Uuid >;
    // TODO: Need to propagate error: Result< UuidVec >
    // TODO: Is this even necessary with composible node_view now?
    template< typename... Nodes >
    [[ maybe_unused ]]
    auto create_children( Uuid const& parent
                        , Nodes const&... children )
        -> UuidVec
    {
        auto rv = UuidVec{};

        // TODO: ( rv.emplace_back( KMAP_TRY( create_child( parent, children ) ) ), ... );
        ( rv.emplace_back( create_child( parent, children ).value() ), ... );

        return rv;
    }
    // TODO [cleanup]: A lot of this API can be rid of and replaced with node_view which uses path.hpp API, bypassing this unnecessary API.
    //                 I think only the fundamentals (core building blocks) really belong in Kmap: create_[child,alias,attr]
    [[ nodiscard ]]
    auto create_descendants( Uuid const& root
                           , Uuid const& selected
                           , Heading const& lineage )
        -> Result< std::vector< Uuid > >;
    [[ nodiscard ]]
    auto create_descendants( Uuid const& root
                           , Heading const& lineage )
        -> Result< std::vector< Uuid > >;
    [[ nodiscard ]]
    auto create_descendants( Heading const& lineage )
        -> Result< std::vector< Uuid > >;
    [[ maybe_unused ]]
    auto create_alias( Uuid const& src
                     , Uuid const& dst )
        -> Result< Uuid >;
    auto create_nodes( std::vector< std::pair< Uuid, std::string > > const& nodes // TODO: Replace with PreparedStatement class?
                     , std::vector< std::pair< Uuid, Uuid > > const& edges )
        -> void; // TODO: Should return bool to indicate succ/fail.
    [[ nodiscard ]]
    auto distance( Uuid const& ancestor
                 , Uuid const& descendant ) const
        -> uint32_t;
    [[ maybe_unused ]]
    auto store_resource( Uuid const& parent
                       , Heading const& heading
                       , std::byte const* data
                       , size_t const& size )
        -> Result< Uuid >;
    [[ maybe_unused ]]
    auto copy_body( Uuid const& src
                  , Uuid const& dst )
        -> Result< void >;
    [[ nodiscard ]] // TODO: I believe nodiscard is the correct attr. User should always check if the copy was successful. 
    auto copy_state( FsPath const& dst )
        -> bool; // TODO: Replace with Result<>
    [[ nodiscard ]]
    auto move_body( Uuid const& src
                  , Uuid const& dst )
        -> Result< void >;
    [[ nodiscard ]]
    auto move_state( FsPath const& dst )
        -> Result< void >;
    auto set_up_db_root()
        -> Result< void >;
    auto set_up_nw_root()
        -> Result< void >;
    [[ maybe_unused ]]
    auto set_ordering_position( Uuid const& id
                              , uint32_t pos )
        -> bool;
    auto update_body( Uuid const& id
                    , std::string const& contents )
        -> Result< void >;
    [[ nodiscard ]]
    auto load_state( FsPath const& db_path )
        -> Result< void >;
    [[ maybe_unused ]]
    auto travel_bottom() // TODO: Add unit test for this.
        -> Uuid;
    [[ maybe_unused ]]
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
    auto reset()
        -> Result< void >;
    auto reset_database()
        -> Result< void >;
    auto reset_network()
        -> Result< void >;
    auto rename( Uuid const& id
               , Heading const& heading )
        -> void;
    [[ nodiscard ]]
    auto exists( Uuid const& id ) const
        -> bool;
    [[ nodiscard ]]
    auto exists( Heading const& heading ) const
        -> bool;
    template< typename... Args >
    [[ nodiscard ]]
    auto exist( Args const&... args ) const -> bool { return ( exists( args ) && ... ); } // Warning: arguments must be explicit for template deduction, so e.g., "/" must be Heading{ "/" }.
    auto reorder_children( Uuid const& parent
                         , std::vector< Uuid > const& children )
        -> Result< void >;
    [[ maybe_unused ]]
    auto select_node( Uuid const& id )
        -> Result< Uuid >;
    [[ maybe_unused ]]
    auto swap_nodes( Uuid const& t1
                   , Uuid const& t2 )
        -> Result< std::pair< Uuid, Uuid > >;
    [[ maybe_unused ]]
    auto select_node( Heading const& heading )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto selected_node() const
        -> Uuid;
    auto load_preview( Uuid const& id )
        -> void;
    auto on_leaving_editor()
        -> Result< void >;
    auto focus_network()
        -> Result< void >;
    auto update_heading( Uuid const& id
                       , Heading const& heading )
        -> Result< void >;
    auto update_title( Uuid const& id
                     , Title const& title )
        -> Result< void >;
    auto update_alias( Uuid const& from
                     , Uuid const& to )
        -> Result< Uuid >;
    auto update_aliases( Uuid const& descendant )
        -> Result< void >;
    // TODO: Belongs in Network, instead?
    auto color_node( Uuid const& id
                   , Color const& color )
        -> void;
    auto color_node( Uuid const& id )
        -> void;
    auto color_all_visible_nodes()
        -> void;
    [[ nodiscard ]]
    auto fetch_ancestor( Uuid const& descendant
                       , InvertedHeading const& heading ) const
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto fetch_ancestral_lineage( Uuid const& root
                                , Uuid const& id
                                , uint32_t const& max ) const
        -> UuidPath;
    [[ nodiscard ]]
    auto fetch_ancestral_lineage( Uuid const& id
                                , uint32_t const& max ) const
        -> UuidPath;
    [[ nodiscard ]]
    auto fetch_body( Uuid const& id ) const
        -> Result< std::string >;
    [[ nodiscard ]]
    auto fetch_child( Uuid const& parent 
                    , Heading const& heading ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_alias_children( Uuid const& parent ) const
        -> std::vector< Uuid >;
    [[ nodiscard ]]
    auto fetch_children( Uuid const& parent ) const
        -> kmap::UuidSet;
    [[ nodiscard ]]
    auto fetch_children( Uuid const& root
                       , Heading const& parent ) const
        -> kmap::UuidSet;
    [[ nodiscard ]]
    auto fetch_parent_children( Uuid const& id ) const
        -> kmap::UuidSet;
    [[ nodiscard ]]
    auto fetch_parent_children_ordered( Uuid const& id ) const
        -> kmap::UuidVec;
    [[ nodiscard ]]
    auto fetch_siblings( Uuid const& id ) const // TODO [cleanup]: view::make( id ) | view::sibling | view::to_node_set( kmap ); // view::sibling => view::make( id ) | view::parent | view::child( view::none_of( id ) )
        -> kmap::UuidSet;
    [[ nodiscard ]]
    auto fetch_siblings_ordered( Uuid const& id ) const
        -> kmap::UuidVec;
    [[ nodiscard ]]
    auto fetch_siblings_inclusive( Uuid const& id ) const
        -> kmap::UuidSet;
    [[ nodiscard ]]
    auto fetch_siblings_inclusive_ordered( Uuid const& id ) const
        -> kmap::UuidVec;
    // TODO [cleanup]: Deprecate in favor of view::make( root ) | view::child | view::fetch_node( kmap ) | view::to_ordered
    [[ nodiscard ]]
    auto fetch_children_ordered( Uuid const& root
                               , Heading const& path ) const
        -> UuidVec;
    [[ nodiscard ]]
    auto fetch_children_ordered( Uuid const& parent ) const
        -> UuidVec;
    [[ nodiscard ]]
    auto fetch_heading( Uuid const& id ) const
        -> Result< Heading >;
    [[ nodiscard ]]
    auto fetch_nodes( Heading const& heading ) const
        -> UuidSet;
    [[ nodiscard ]]
    auto fetch_parent( Uuid const& child ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_parent( Uuid const& root 
                     , Uuid const& child ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_title( Uuid const& id ) const
        -> Result< Title >;
    // [[ nodiscard ]]
    // auto fetch_nodes() const
    //     -> UuidUnSet;
    [[ nodiscard ]]
    auto descendant_leaves( Uuid const& root ) const // TODO [cleanup]: Deprecate in favor of view::make( root ) | view::desc | view::leaf | view::to_node_set
        -> std::vector< Uuid >;
    [[ nodiscard ]]
    auto descendant_ipaths( Uuid const& root ) const // TODO [cleanup]: Deprecate in favor of view::make( root ) | view::desc | view::to_path | view::reverse
        -> std::vector< HeadingPath >;
    [[ nodiscard ]]
    auto descendant_paths( Uuid const& root ) const // TODO [cleanup]: Deprecate in favor of view::make( root ) | view::desc | view::to_path
        -> std::vector< HeadingPath >;
    [[ nodiscard ]]
    auto fetch_leaf( Uuid const& root // TODO: Rename to fetch_descendant? Reason being that a "leaf" refers to a terminal node in the tree. This is not necessarily the case with this function.
                   , Uuid const& selected
                   , Heading const& heading ) const
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto fetch_descendant( Uuid const& root
                         , Heading const& heading ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_descendant( Heading const& heading ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_descendant( Uuid const& root 
                         , Uuid const& selected
                         , Heading const& heading ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_descendants( Uuid const& root 
                          , Uuid const& selected
                          , Heading const& heading ) const
        -> Result< UuidVec >;
    [[ nodiscard ]]
    auto fetch_descendants( Heading const& heading ) const
        -> Result< UuidVec >;
    [[ nodiscard ]]
    auto fetch_leaf( Heading const& heading ) const // TODO: Rename to fetch_descendant?
        -> Optional< Uuid >;
    // TODO: Replace with mirror()...
    [[ nodiscard ]]
    auto fetch_or_create_descendant( Uuid const& root
                                   , Uuid const& selected
                                   , Heading const& heading ) 
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_or_create_descendant( Uuid const& root
                                   , Heading const& heading ) 
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_or_create_descendant( Heading const& heading ) 
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto fetch_or_create_leaf( Uuid const& root // TODO: Rename to fetch_descendant?
                             , Heading const& heading ) 
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto fetch_or_create_leaf( Heading const& heading ) // TODO: Rename to fetch_descendant?
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto fetch_ordering_position( Uuid const& node ) const
        -> Result< uint32_t >;
    [[ nodiscard ]]
    auto fetch_aliased_ancestry( Uuid const& id ) const
        -> std::vector< Uuid >;
    // TODO [cleanup]: Belongs in node_view, I think: root | view::child( view::all_of( n1, n2 ) ) | view::are_siblings( kmap );
    [[ nodiscard ]]
    auto are_siblings( Uuid const& n1 // TODO: Replace with initializer_list to allow for arbitrary number of nodes?
                     , Uuid const& n2 ) const
        -> bool;
    [[ nodiscard ]]
    auto has_alias( Uuid const& node ) const
        -> bool;
    [[ nodiscard ]]
    auto has_descendant( Uuid const& ancestor 
                       , std::function< bool( Uuid const& ) > const& pred ) const
        -> bool;
    [[ nodiscard ]]
    auto is_ancestry_aliased( Uuid const& id ) const
        -> bool;
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
    auto is_child( std::vector< Uuid > const& parents
                 , Uuid const& child ) const
        -> bool;
    [[ nodiscard ]]
    auto is_alias( Uuid const& id ) const
        -> bool;
    [[ nodiscard ]]
    auto is_alias( Uuid const& src
                 , Uuid const& dst ) const
        -> bool;
    [[ nodiscard ]]
    auto is_top_alias( Uuid const& id ) const
        -> bool;
    // TODO [cleanup]: Can be replaced by node_view.
    [[ nodiscard ]]
    auto is_leaf_node( Uuid const& id ) const
        -> bool;
    [[ nodiscard ]]
    auto is_lineal( Uuid const& ancestor
                  , Uuid const& descendant ) const
        -> bool;
    [[ nodiscard ]]
    auto is_lineal( Uuid const& ancestor
                  , Heading const& descendant ) const
        -> bool;
    // TODO [cleanup]: Can be replaced by node_view: view::make( root ) | view::desc( node ) | view::exists( kmap )
    [[ nodiscard ]]
    auto is_in_tree( Uuid const& root
                   , Uuid const& node ) const
        -> bool;
    // TODO [cleanup]: Can be replaced by node_view: view::make( ancestor ) | view::direct_desc( path ) | view::exists( kmap )
    [[ nodiscard ]]
    auto is_direct_descendant( Uuid const& ancestor
                             , Heading const& path ) const
        -> bool;
    // TODO [cleanup]: Can be replaced by node_view: view::make( root ) | view::ancestor( node ) | view::exists( kmap )
    [[ nodiscard ]]
    auto is_ancestor( Uuid const& root
                    , Uuid const& node ) const
        -> bool;
    [[ nodiscard ]]
    auto is_root( Uuid const& node ) const
        -> bool;
    auto jump_to( Uuid const& id )
        -> Result< void >;
    auto jump_in()
        -> void;//Optional< Uuid >;
    auto jump_out()
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto jump_stack()
        -> JumpStack&;
    [[ nodiscard ]]
    auto jump_stack() const
        -> JumpStack const&;
    auto root_view() const
        -> view::Intermediary;
    [[ maybe_unused ]]
    auto move_node( Uuid const& from
                  , Uuid const& to )
        -> Result< Uuid >;
    [[ maybe_unused ]]
    auto move_node( Uuid const& from
                  , Heading const& to )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto resolve( Uuid const& id ) const
        -> Uuid;
    auto erase_node( Uuid const& id )
        -> Result< Uuid >;
    auto erase_alias( Uuid const& id ) // TODO: Rename to erase_internal_alias - more precise, and simply use erase_node. If it must be an alias, use is_alias(); erase_node().
        -> Result< Uuid >;
    auto erase_aliases_to( Uuid const& id )
        -> Result< void >;
    auto erase_attr( Uuid const& id )
        -> Result< Uuid >;
    auto update( Uuid const& id )
        -> void;
    [[ nodiscard ]]
    auto node_fetcher() const
        -> NodeFetcher;
    [[ nodiscard ]]
    auto fetch_attr_node( Uuid const& id ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto create_attr_node( Uuid const& parent)
        -> Result< Uuid >;
    // TODO [cleanup]: move to attr.hpp?
    [[ nodiscard ]]
    auto fetch_genesis_time( Uuid const& id ) const
        -> Optional< uint64_t >; // TODO: Result< uint64_t >
    [[ nodiscard ]]
    auto get_appropriate_node_font_face( Uuid const& id ) const
        -> std::string;
    [[ nodiscard ]]
    auto timer()
        -> chrono::Timer&;
    [[ nodiscard ]]
    auto timer() const
        -> chrono::Timer const&;

protected:
    auto create_child_internal( Uuid const& parent
                              , Uuid const& child
                              , Heading const& heading
                              , Title const& title )
        -> kmap::Result< void >;
    auto create_internal_alias( Uuid const& src
                              , Uuid const& dst )
        -> Result< Uuid >;
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

private:
    std::unique_ptr< Environment > env_; // pimpl.
    std::unique_ptr< Database > database_; // pimpl.
    std::unique_ptr< Canvas > canvas_; // pimpl.
    std::unique_ptr< Network > network_; // pimpl.
    std::unique_ptr< Cli > cli_; // pimpl.
    std::unique_ptr< TextArea > text_area_ = {}; // pimpl.
    std::unique_ptr< EventStore > event_store_ = {}; // pimpl.
    std::unique_ptr< OptionStore > option_store_ = {}; // pimpl.
    std::unique_ptr< chrono::Timer > timer_ = {}; // pimpl.
    std::unique_ptr< db::Autosave > autosave_ = {}; // pimpl.
    JumpStack jump_stack_ = {}; // TODO: Rather belongs in Environment, as it's tied to the kmap instance.
    // TODO: Refactor alias functionality into e.g., AliasStore.
    AliasSet alias_set_ = {};
    AliasChildSet alias_child_set_ = {};
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
