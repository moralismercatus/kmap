/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "canvas.hpp"

#include "contract.hpp"
#include "error/master.hpp"
#include "error/network.hpp"
#include "error/node_manip.hpp"
#include "event/event.hpp"
#include "js_iface.hpp"
#include "option/option.hpp"
#include "path.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"
#include "util/window.hpp"
#include "utility.hpp"

#include <emscripten.h>
#include <emscripten/val.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/replace.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/stride.hpp>
#include <range/v3/view/transform.hpp>

#include <string>
#include <regex>

using namespace ranges;

namespace kmap {

auto operator<<( std::ostream& lhs 
               , Dimensions const& rhs )
    -> std::ostream&
{
    lhs << io::format( "top:{}, bottom:{}, left:{}, right:{}", rhs.top, rhs.bottom, rhs.left, rhs.right );

    return lhs;
}

auto to_string( Orientation const& orientation )
    -> std::string
{
    switch( orientation )
    {
        case Orientation::horizontal: return "horizontal";
        case Orientation::vertical: return "vertical";
    }
}

template<>
auto from_string( std::string const& s )
    -> Result< Orientation >
{
    auto rv = KMAP_MAKE_RESULT_EC( Orientation, error_code::common::conversion_failed );

    if( s == "horizontal" )
    {
        rv = Orientation::horizontal;
    }
    else if( s == "vertical" )
    {
        rv = Orientation::vertical;
    }

    return rv;
}

auto fetch_overlay_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "/meta.setting.window.canvas.overlay";

    return kmap.fetch_or_create_descendant( abs_root, abs_root, root_path );
}

auto fetch_subdiv_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "/meta.setting.window.canvas.subdivision";

    return kmap.fetch_or_create_descendant( abs_root, abs_root, root_path );
}

auto fetch_window_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "/meta.setting.window";

    return kmap.fetch_or_create_descendant( abs_root, abs_root, root_path );
}

Canvas::Canvas( Kmap& kmap )
    : kmap_{ kmap }
{
}

Canvas::~Canvas()
{
    try
    {
fmt::print( "Canvas::~create_html_canvas\n" );
        for( auto const& elem_id : canvas_element_stack_
                                 | ranges::views::reverse )
        {
            KTRYE( js::erase_child_element( to_string( elem_id ) ) );
        }
        
        if( canvas_root_ )
        {
            KTRYE( kmap_.erase_node( canvas_root_.value() ) );
        }
    }
    catch( std::exception& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto Canvas::fetch_canvas_root()
    -> Result< Uuid >
{
    if( !canvas_root_ )
    {
        auto const abs_root = kmap_.root_node_id();
        auto const root_path = "/meta.setting.window.canvas";

        canvas_root_ = KTRY( kmap_.fetch_or_create_descendant( abs_root, abs_root, root_path ) );
    }

    return canvas_root_.value();
}

auto Canvas::init_event_callbacks()
    -> Result< void >
{
    return js::eval_void( "window.onresize = function(){ kmap.canvas().update_all_panes(); };" );
}

auto Canvas::install_events()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap_.event_store();

    KTRY( estore.install_subject( "window" ) );
    KTRY( estore.install_verb( "scaled" ) );

    KTRY( js::eval_void( "window.onresize = function(){ kmap.event_store().fire_event( to_VectorString( [ 'subject.window', 'verb.scaled' ] ) ); };" ) );

    #if 0

    KMAP_TRY( estore.install_subject( "keyboard.key.c" ) );
    KMAP_TRY( estore.install_verb( "depressed" ) );
    KMAP_TRY( estore.install_object( "network" ) );
    KMAP_TRY( estore.install_listener( "network.travel_left"
                                     , event::any_of{ event::action{ "keyboard.key.down" } }
                                     , event::all_of{ event::any_of{ event::object{ "keyboard.h" } }
                                                    , event::excl{ event::state{ "keyboard", "keyboard.key.down" } } } );
    KMAP_TRY( estore.install_listener( "network.travel_left"
                                     , event::any_of{ event::action{ "keyboard.key.down" } }
                                     , event::all_of{ event::any_of{ event::object{ "keyboard.h" } }
                                                    , event::excl{ event::state{ "keyboard", "keyboard.key.down" } } } );
                                                    // , event::none_of{ event::state{ "keyboard.ctrl", "keyboard.key.down" }
                                                    //                 , event::state{ "keyboard.alt", "keyboard.key.down" } ) );
    KMAP_TRY( estore.install_listener( "network.travel_left"
                                     , event::any_of{ "keyboard.key.down" }
                                     , event::any_of{ event::all_of{ "keyboard.key.c"
                                                                   , "keyboard.key.ctrl.state.down" }
                                                    , event::all_of{ "keyboard.key.c.state.down"
                                                                   , "keyboard.key.ctrl" } } ) );
    #endif // 0

    rv = outcome::success();

    return rv;
}

auto Canvas::install_options()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& opt = kmap_.option_store();

    // Network
    KMAP_TRY( opt.install_option( "canvas.network.background.color"
                                , "Sets the background color for the nextwork pane."
                                , "\"#222222\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().network_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    // Preview
    KMAP_TRY( opt.install_option( "canvas.preview.background.color"
                                , "Sets the background color for the preview pane."
                                , "\"#222222\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.preview.text.color"
                                , "Sets the text color for the preview pane."
                                , "\"white\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.color = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.preview.text.size"
                                , "Text size."
                                , "\"large\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.fontSize = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.preview.scrollbar"
                                , "Specify scroll bar."
                                , "\"auto\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.overflow = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.preview.whitespace_wrap"
                                , "Specify scroll behavior."
                                , "\"nowrap\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.whiteSpace = option_value;" ) );
    // Editor
    KMAP_TRY( opt.install_option( "canvas.editor.background.color"
                                , "Sets the background color for the editor pane."
                                , "\"#222222\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.editor.text.color"
                                , "Sets the text color for the editor pane."
                                , "\"white\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.color = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.editor.text.size"
                                , "Text size."
                                , "\"large\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.fontSize = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.editor.scrollbar"
                                , "Specify scroll behavior."
                                , "\"auto\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.overflow = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.editor.whitespace_wrap"
                                , "Specify scroll behavior."
                                , "\"nowrap\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.whiteSpace = option_value;" ) );
    // CLI
    KMAP_TRY( opt.install_option( "canvas.cli.background.color"
                                , "Sets the background color for the cli pane."
                                , "\"#222222\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.cli.text.color"
                                , "Sets the text color for the cli pane."
                                , "\"white\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value_or_throw() ).style.color = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.cli.text.size"
                                , "Text size."
                                , "\"large\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value_or_throw() ).style.fontSize = option_value;" ) );
    // Completion Box
    KMAP_TRY( opt.install_option( "canvas.completion_box.background.color"
                                , "Sets the background color for the completion box."
                                , "\"#333333\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.completion_box.text.color"
                                , "Sets the text color for the completion box popup."
                                , "\"white\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.color = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.completion_box.padding"
                                , "Sets the padding between edge of box and internal text."
                                , "\"0px\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.padding = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.completion_box.border.radius"
                                , "Sets the rounding radius for the corners of the box."
                                , "\"5px\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.borderRadius = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.completion_box.border.style"
                                , "Border style"
                                , "\"outset\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.borderStyle = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.completion_box.border.width"
                                , "Width of border."
                                , "\"thin\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.borderWidth = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.completion_box.scrollbar"
                                , "Specify scroll behavior."
                                , "\"auto\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.overflow = option_value;" ) );
    // I think all canvas items are absolute... I think this gets encoded when they are created. Probably doesn't belong here.
    KMAP_TRY( opt.install_option( "canvas.completion_box.position_type"
                                , "Sets the rounding radius for the corners of the box."
                                , "\"absolute\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.position = option_value;" ) );
    KMAP_TRY( opt.install_option( "canvas.completion_box.placement_order"
                                , "Specifies order in which box will be placed vis-a-vis other canvas elements."
                                , "\"9999\""
                                , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.zIndex = option_value;" ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::complete_path( std::string const& path )
    -> StringVec
{
    auto rv = StringVec{};
    if( auto const canvas_root = fetch_canvas_root()
      ; canvas_root )
    {
        auto const inflated = std::regex_replace( path, std::regex{ "\\." }, ".subdivision." );
        if( auto const completed = complete_path_reducing( kmap_ 
                                                         , canvas_root.value()
                                                         , canvas_root.value()
                                                         , inflated )
          ; completed )
        {
            for( auto const completed_value = completed.value()
               ; auto const& c : completed_value )
            {
                auto const deflated = std::regex_replace( c.path, std::regex{ "\\.subdivision\\." }, "." );
                if( is_pane( c.target ) )
                {
                    rv.emplace_back( deflated );
                }
            }
        }
    }
    
    return rv;
}

auto Canvas::is_overlay( Uuid const& node )
    -> bool
{
    auto rv = false;

    if( auto const overlay_root = fetch_overlay_root( kmap_ )
      ; overlay_root )
    {
        rv = kmap_.is_ancestor( overlay_root.value(), node );
    }

    return rv;
}

auto Canvas::is_pane( Uuid const& node )
    -> bool
{
    using emscripten::val;

    auto rv = false;

    if( auto const canvas_root = fetch_canvas_root()
      ; canvas_root )
    {
        rv = kmap_.is_lineal( canvas_root.value(), node )
          && ( canvas_root.value() == node || kmap_.fetch_heading( kmap_.fetch_parent( node ).value() ).value() == "subdivision" )
          && kmap_.is_child( node
                           , "orientation"
                           , "base"
                           , "hidden"
                           , "subdivision" )
          && js::fetch_element_by_id< val >( to_string( node ) ).has_value();
    }

    return rv;
}

auto Canvas::set_breadcrumb( UuidVec const& bc )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );
    
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // TODO: Re-enable
            // BC_ASSERT( js::exists( breadcrumb_uuid ) );
        })
    ;

    rv = outcome::success();
    return rv;

    // TODO: What if the selected node is in the table?
    // TODO: More generally, what if kmap_.delete has this situation? Is it handled? TC?
    // Remove all subdivisions to start with clean slate.
    KMAP_TRY( delete_subdivisions( breadcrumb_table_pane() ) );

    io::print( "bc.size(): {}\n", bc.size() );

    auto accumulated_width = decltype( Division::base ){};

    io::print( "titles.size(): {}\n", bc.size() );

    // TODO:
    // There's a major problem here. Namely, that create_child will be called, and not just once, but repeatedly.
    // This will not only invalidate the DB cache, but also require multiple SQL writes (via delete child / create child)
    // So... what's to be done?
    // In a way, I don't want these to even be saved to the DB.
    for( auto const& [ index, target ] : views::enumerate( bc ) )
    {
        auto const title = KMAP_TRY( kmap_.fetch_title( target ) );
        io::print( "creating subdivision: {}\n", index );
        // Determine width. Accumulate widths to determine base of fill div.
        auto const subdiv = KMAP_TRY( subdivide( breadcrumb_table_pane()
                                               , fmt::format( "{}", index ) // Simply use enumeration value as pane heading.
                                               , Division{ Orientation::horizontal, accumulated_width, false }
                                               , "div" ) );
        io::print( "created subdivision: {}, {}\n", index, to_string( subdiv ) );
        auto div = KMAP_TRY( js::fetch_element_by_id< val >( to_string( subdiv ) ) );

        div.set( "innerText", fmt::format( "{}", title ) );
        div.set( "style", fmt::format( "color: black", title ) );
        div.set( "onclick", fmt::format( "function(){{ kmap.select_node( '{}' ).throw_on_error(); }}", to_string( target ) ) );
        accumulated_width += 0.10;//calc_text_width( title );
    }

    // TODO: fill remaining space with fill div, based at accumulated_width.
    KMAP_TRY( update_pane_descending( breadcrumb_pane() ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::subdivide( Uuid const& parent_pane
                      , Uuid const& pane_id
                      , Heading const& heading
                      , Division const& subdiv
                      , std::string const& elem_type )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( is_pane( parent_pane ), error_code::canvas::invalid_subdiv );
    KMAP_ENSURE( is_valid_heading( heading ), error_code::network::invalid_heading );

    auto const parent_pane_subdivn = KMAP_TRY( kmap_.fetch_child( parent_pane, "subdivision" ) );

    rv = create_subdivision( parent_pane_subdivn
                           , pane_id
                           , heading
                           , subdiv
                           , elem_type );

    return rv;
}

auto Canvas::subdivide( Uuid const& parent_pane
                      , Heading const& heading
                      , Division const& subdiv
                      , std::string const& elem_type )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( is_pane( parent_pane ), error_code::canvas::invalid_subdiv );
    KMAP_ENSURE( is_valid_heading( heading ), error_code::network::invalid_heading );

    auto const parent_pane_subdivn = KTRY( view::make( parent_pane )
                                         | view::child( "subdivision" )
                                         | view::fetch_node( kmap_ ) );

    rv = create_subdivision( parent_pane_subdivn
                           , heading
                           , subdiv
                           , elem_type );

    return rv;
}

auto Canvas::update_all_panes()
    -> Result< void >
{
    return update_pane( KMAP_TRY( fetch_canvas_root() ) );
}

auto Canvas::update_pane( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    if( auto const parent = fetch_parent_pane( pane )
      ; parent )
    {
        rv = update_pane_descending( parent.value() );
    }
    else
    {
        rv = update_pane_descending( pane );
    }
    
    return rv;
}

// TODO: Create repair pass that ensures all canvas elements have an associated document.getElementById() entry.
auto Canvas::update_pane_descending( Uuid const& root ) // TODO: Lineal< window_root, pane >
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );
    auto const subdivn = KMAP_TRY( pane_subdivision( root ) );
    
    // io::print( "updating pane: {}\n", kmap_.absolute_path_flat( root ) );

    KMAP_ENSURE( is_pane( root ), error_code::network::invalid_node );

    auto style = KMAP_TRY( js::eval< val >( io::format( "return document.getElementById( '{}' ).style;", root ) ) ); 

    auto const dims = KMAP_TRY( dimensions( root ) );
    io::print( "'{}' dims: {}\n", KMAP_TRYE( kmap_.fetch_heading( root ) ), dims );
    BC_ASSERT( dims.bottom >= dims.top );
    BC_ASSERT( dims.right >= dims.left );
    style.set( "position", "absolute" );
    style.set( "top", io::format( "{}px", dims.top ) );
    style.set( "height", io::format( "{}px", dims.bottom - dims.top ) );
    style.set( "left", io::format( "{}px", dims.left ) );
    style.set( "width", io::format( "{}px", dims.right - dims.left ) );
    style.set( "border", "1px solid black" );
    // TODO: subdivision style map? for each: style.set( key, val );
    //       Is there any way to sensibly pull this info from setting.option?

    for( auto const& subdiv : kmap_.fetch_children( subdivn ) )
    {
        KMAP_TRY( update_pane_descending( subdiv ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::update_panes()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const canvas_root = KMAP_TRY( fetch_canvas_root() );

    KMAP_ENSURE( is_pane( canvas_root ), error_code::network::invalid_node );

    KMAP_TRY( update_pane_descending( canvas_root ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::rebase_internal( Uuid const& pane
                            , float const base )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_pane( pane ), error_code::network::invalid_node );

    auto const basen = KMAP_TRY( kmap_.fetch_descendant( pane, pane, "/base" ) );

    KMAP_TRY( kmap_.update_body( basen, io::format( "{:.4f}", base ) ) );

    // Ensure network displays them as ordered according to their position.
    {
        auto const parent = KMAP_TRY( kmap_.fetch_parent( pane ) );
        auto const children = kmap_.fetch_children_ordered( parent )
                            | actions::sort( [ & ]( auto const& lhs, auto const& rhs ){ return pane_base( lhs ).value() < pane_base( rhs ).value(); } );
        KMAP_TRY( kmap_.reorder_children( parent, children ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::rebase( Uuid const& pane
                   , float const base )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( rebase_internal( pane, base ) );
    KMAP_TRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reorient_internal( Uuid const& pane
                              , Orientation const& orientation )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    auto const orientn = KMAP_TRY( kmap_.fetch_descendant( pane, pane, "/orientation" ) );

    KMAP_TRY( kmap_.update_body( orientn, to_string( orientation ) ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::orient( Uuid const& pane
                   , Orientation const& orientation )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( reorient_internal( pane, orientation ) );
    KMAP_TRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reorient( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const prev_orient = KMAP_TRY( pane_orientation( pane ) );
    auto const orientation = [ & ]
    {
        switch( prev_orient )
        {
            case Orientation::horizontal: return Orientation::vertical;
            case Orientation::vertical: return Orientation::horizontal;
        }
    }();

    rv = orient( pane, orientation );

    return rv;
}

auto Canvas::hide( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( hide_internal( pane, true ) );
    KMAP_TRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::hide_internal( Uuid const& pane
                          , bool const hidden )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE_MSG( kmap_.exists( pane ), error_code::network::invalid_node, to_string_elaborated( kmap_, pane ) );
    KMAP_ENSURE_MSG( is_pane( pane ), error_code::network::invalid_node, to_string_elaborated( kmap_, pane ) );

    auto const hiddenn = KMAP_TRY( kmap_.fetch_descendant( pane, pane, "/hidden" ) );
    auto const hidden_str = to_string( hidden );

    KMAP_TRY( kmap_.update_body( hiddenn, hidden_str ) );
    
    KMAP_TRY( js::eval_void( io::format( "document.getElementById( '{}' ).hidden={};", to_string( pane ), hidden_str ) ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::create_html_canvas( Uuid const& id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

fmt::print( "Canvas::create_html_canvas( {} );\n", to_string( id ) );
    KTRY( js::create_html_canvas( to_string( id ) ) );

    canvas_element_stack_.emplace_back( id );

    rv = outcome::success();

    return rv;
}

auto Canvas::create_html_child_element( std::string const& elem_type
                                      , Uuid const& parent_id
                                      , Uuid const& child_id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( js::eval_void( io::format( "let child_div = document.createElement( '{}' );"
                                     "child_div.id = '{}';"
                                     "child_div.tabIndex= {};"
                                     "let parent_div = document.getElementById( '{}' );"
                                     "parent_div.appendChild( child_div );" 
                                   , elem_type
                                   , to_string( child_id )
                                   , next_tabindex_++
                                   , to_string( parent_id ) ) ) );

    canvas_element_stack_.emplace_back( child_id );

    rv = outcome::success();

    return rv;
}

auto Canvas::reset()
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( create_html_canvas( util_canvas_uuid ) );

    auto const canvas     = KTRY( reset_root() );
                            //KTRY( reset_breadcrumb( canvas ) );
    auto const workspace  = KTRY( subdivide( canvas, workspace_pane(), "workspace", Division{ Orientation::vertical, 0.025f, false }, "div" ) );
                            KTRY( subdivide( workspace, network_pane(), "network", Division{ Orientation::horizontal, 0.000f, false }, "div" ) );
    auto const text_area  = KTRY( subdivide( workspace, text_area_pane(), "text_area", Division{ Orientation::horizontal, 0.660f, false }, "div" ) );
                            KTRY( subdivide( text_area, editor_pane(), "editor", Division{ Orientation::horizontal, 0.000f, true }, "textarea" ) );
                            KTRY( subdivide( text_area, preview_pane(), "preview", Division{ Orientation::horizontal, 1.000f, false }, "div" ) );
                            KTRY( subdivide( canvas, cli_pane(), "cli", Division{ Orientation::horizontal, 0.975f, false }, "input" ) );


    KMAP_TRY( create_overlay( completion_overlay(), "completion_overlay", "div" ) );

    KMAP_TRY( update_panes() );
    KMAP_TRY( init_event_callbacks() );
    KMAP_TRY( install_events() );

    rv = outcome::success();

    return rv;
}

auto Canvas::reset_breadcrumb( Uuid const& supdiv )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( is_pane( supdiv ), error_code::canvas::invalid_pane );

    /*auto const breadcrumb = */KMAP_TRY( subdivide( supdiv, breadcrumb_pane(), "breadcrumb", Division{ Orientation::horizontal, 0.000f, false }, "div" ) );

    // How many cells do we allow? 10? To a certain extent, it depends on screen size.
    // For now, 10 sounds reasonable.
    // for( auto const& i : views::indices( 10 ) )
    // {
    //     KMAP_TRY( subdivide( breadcrumb, breadcrumb_table_pane(), fmt::format( "{}", i ), Division{ Orientation::vertical, 0.000f, true }, "div" ) );
    // }
                        
    return rv;
}

auto Canvas::make_subdivision( Uuid const& target
                             , Division const& subdiv )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_pane( target ) );
            }
        })
    ;

    KTRY( kmap_.fetch_or_create_descendant( target, target, "/orientation" ) );
    KTRY( kmap_.fetch_or_create_descendant( target, target, "/base" ) );
    KTRY( kmap_.fetch_or_create_descendant( target, target, "/hidden" ) );
    KTRY( kmap_.fetch_or_create_descendant( target, target, "/subdivision" ) );
    KTRY( reorient_internal( target, subdiv.orientation ) );
    KTRY( rebase_internal( target, subdiv.base ) );
    KTRY( hide_internal( target, subdiv.hidden ) );
    // KTRY( update_pane( target ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::create_overlay( Uuid const& id
                           , std::string const& heading
                           , std::string const& elem_type )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_overlay( rv.value() ) );
            }
        })
    ;

    auto const canvas_root = KMAP_TRY( fetch_canvas_root() );
    auto const overlay_root = KMAP_TRY( fetch_overlay_root( kmap_ ) );
    auto const overlay = KMAP_TRY( kmap_.create_child( overlay_root, id, heading ) );

    KTRY( create_html_child_element( elem_type, canvas_root, overlay ) );

    rv = overlay;

    return rv;
}

auto Canvas::create_subdivision( Uuid const& parent
                               , Uuid const& child
                               , std::string const& heading
                               , Division const& subdiv
                               , std::string const& elem_type )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_pane( rv.value() ) );
            }
        })
    ;

    auto const parent_pane = KMAP_TRY( fetch_parent_pane( parent ) );
    auto const subdivn = KMAP_TRY( kmap_.create_child( parent, child, heading ) );

    KTRY( create_html_child_element( elem_type, parent_pane, subdivn ) );
    KTRY( make_subdivision( subdivn, subdiv ) );

    rv = subdivn;

    return rv;
}

auto Canvas::create_subdivision( Uuid const& parent
                               , std::string const& heading
                               , Division const& subdiv
                               , std::string const& elem_type )
    -> Result< Uuid >
{
    return create_subdivision( parent 
                             , gen_uuid()
                             , heading
                             , subdiv
                             , elem_type );
}

// TODO: This needs to delete all subdivisions first.
auto Canvas::reset_root()
    -> Result< Uuid >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Uuid );

    {
        auto const canvas_root = KMAP_TRY( fetch_canvas_root() );

        if( js::element_exists( to_string( canvas_root ) ) )
        {
            KMAP_TRY( delete_pane( canvas_root ) );
        }
    }

    {
        auto const canvas_root = KMAP_TRY( fetch_canvas_root() );

        // TODO: Shouldn't... this be a child of the 'canvas' element that we created?
        //       Unless... we don't actually use the 'canvas' type, but rather divs only.
        //       And... what is the 'canvas' element's purpose then? Anyway, 'div's will simply things, as I'm familiar with those.
        KTRY( js::eval_void( io::format( "let canvas_div = document.createElement( 'div' );"
                                         "canvas_div.id = '{}';"
                                         "let body_tag = document.getElementsByTagName( 'body' )[ 0 ];"
                                         "body_tag.appendChild( canvas_div );" 
                                       , to_string( canvas_root ) ) ) );

        canvas_element_stack_.emplace_back( canvas_root );

        KMAP_TRY( make_subdivision( canvas_root, { Orientation::horizontal, 0.0f, false } ) );

        rv = canvas_root;
    }

    fmt::print( "finished: Canvas::reset_root\n" );

    return rv;
}

auto Canvas::reveal( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( hide_internal( pane, false ) );
    KMAP_TRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::width() const
    -> uint32_t
{
    return window::inner_width();
}

auto Canvas::height() const
    -> uint32_t
{
    return window::inner_height();
}

auto Canvas::pane_orientation( Uuid const& subdiv )
    -> Result< Orientation >
{
    auto rv = KMAP_MAKE_RESULT( Orientation );

    KMAP_ENSURE( is_pane( subdiv ), error_code::network::invalid_node );

    auto const orient_node = KMAP_TRY( kmap_.fetch_child( subdiv, "orientation" ) );
    auto const orient_body = KMAP_TRY( kmap_.fetch_body( orient_node ) );

    rv = from_string< Orientation >( orient_body );

    return rv;
}

auto Canvas::pane_base( Uuid const& subdiv )
    -> Result< float >
{
    auto rv = KMAP_MAKE_RESULT( float );

    KMAP_ENSURE( is_pane( subdiv ), error_code::network::invalid_node );

    auto const base_node = KMAP_TRY( kmap_.fetch_child( subdiv, "base" ) );
    auto const base_body = KMAP_TRY( kmap_.fetch_body( base_node ) );
    
    try // TODO: abstract into utility that returns Result< float >
    {
        rv = std::stof( base_body );
    }
    catch( std::exception const& e )
    {
        rv = KMAP_MAKE_ERROR( error_code::common::conversion_failed );
    }

    return rv;
}

auto Canvas::pane_hidden( Uuid const& pane )
    -> Result< bool >
{
    // TODO: postcond div.hidden == pane.hidden
    auto rv = KMAP_MAKE_RESULT( bool );

    KMAP_ENSURE( is_pane( pane ), error_code::network::invalid_node );

    auto const hidden_node = KMAP_TRY( kmap_.fetch_child( pane, "hidden" ) );
    auto const hidden_body = KMAP_TRY( kmap_.fetch_body( hidden_node ) );

    rv = KMAP_TRY( from_string< bool >( hidden_body ) );

    return rv;
}

auto Canvas::pane_subdivision( Uuid const& subdiv )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE_MSG( is_pane( subdiv ), error_code::canvas::invalid_pane, kmap_.fetch_heading( subdiv ).value() );

    rv = kmap_.fetch_child( subdiv, "subdivision" );
    
    return rv;
}

auto Canvas::pane_path( Uuid const& subdiv )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const canvas_root = KMAP_TRY( fetch_canvas_root() );
    auto const ids = kmap_.absolute_path_uuid( KMAP_TRY( make< Lineal >( kmap_, canvas_root, subdiv ) ) );

    KMAP_ENSURE( is_pane( subdiv ), error_code::network::invalid_node );

    auto const headings = ids
       | views::stride( 2 ) // Drop "subdivision" from path.
       | views::transform( [ & ]( auto const& e ){ return kmap_.fetch_heading( e ).value(); } )
        | to< StringVec >();
    rv = headings
       | views::join( '.' )
       | to< std::string >();

    return rv;
}

auto Canvas::delete_pane( Uuid const& pane )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );
    KMAP_ENSURE( js::element_exists( to_string( pane ) ), error_code::canvas::invalid_pane );
    
    for( auto const& sp : KMAP_TRY( fetch_subdivisions( pane ) ) )
    {
        KMAP_TRY( delete_pane( sp ) );
    }

    KMAP_TRY( js::eval_void( fmt::format( "document.getElementById( '{}' ).remove();", to_string( pane ) ) ) );
    KMAP_TRY( kmap_.erase_node( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::delete_subdivisions( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    for( auto const children = view::make( pane )
                             | view::child( "subdivision" )
                             | view::child
                             | view::to_node_set( kmap_ )
        ; auto const& child : children )
    {
        KMAP_TRY( delete_pane( child ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::dimensions( Uuid const& target )
    -> Result< Dimensions >
{
    auto rv = KMAP_MAKE_RESULT( Dimensions );
    auto const canvas_root = KMAP_TRY( fetch_canvas_root() ); 

    KMAP_ENSURE( kmap_.is_lineal( canvas_root, target ), error_code::network::invalid_lineage );

    if( target == canvas_root )
    {
        rv = Dimensions{ .top=0
                       , .bottom=height()
                       , .left=0
                       , .right=width() };
    }
    else
    {
        KMAP_ENSURE( is_pane( target ), error_code::network::invalid_node );

        auto const compute_percents = [ & ]( auto const& siblings )
        {
            auto const sibmap = siblings
                              | views::remove_if( [ & ]( auto const& e ) { return pane_hidden( e ).value(); } )
                              | views::transform( [ & ]( auto const& e ){ return std::pair{ pane_base( e ).value(), e }; } )
                              | to< std::map< float, Uuid > >();
            if( auto const target_it = find_if( sibmap, [ & ]( auto const& e ){ return e.second == target; } )
              ; target_it != end( sibmap ) )
            {
                auto const first = [ & ]
                {
                    if( target_it == begin( sibmap ) )
                    {
                        return 0.0f;
                    }
                    else
                    {
                        return target_it->first;
                    }
                }();
                auto const second = [ & ]
                {
                    if( target_it == prev( end( sibmap ) ) )
                    {
                        return 1.0f;
                    }
                    else
                    {
                        auto const next_it = next( target_it );

                        return next_it->first; // TODO: This may cause overlap. Should it be e.g., `prev_it->first - 0.01f`?
                    }
                }();

                return std::pair{ first, second };
            }
            else
            {
                return std::pair{ 0.0f, 1.0f };
            }
        };
        auto const parent_pane = KMAP_TRY( fetch_parent_pane( target ) );
        auto const orientation = KMAP_TRY( pane_orientation( parent_pane ) );
        auto const siblings = view::make( target )
                            | view::parent
                            | view::child
                            | view::to_node_set( kmap_ )
                            | act::order( kmap_ );
        auto const percs = compute_percents( siblings );
        auto const pdims = KMAP_TRY( dimensions( parent_pane ) );

        switch( orientation )
        {
            case Orientation::horizontal:
            { 
                // TODO: Avoid the cast, percent value should never be over 1.0, so casting shouldn't be an issue.
                rv = Dimensions{ .top=( static_cast< uint32_t >( ( pdims.bottom - pdims.top ) * percs.first ) ) 
                               , .bottom=( static_cast< uint32_t >( ( pdims.bottom - pdims.top ) * percs.second ) )
                               , .left=( 0 )
                               , .right=( pdims.right - pdims.left ) };
                break;
            }
            case Orientation::vertical:
            {
                // TODO: Avoid the cast, percent value should never be over 1.0, so casting shouldn't be an issue.
                rv = Dimensions{ .top=( 0 )
                               , .bottom=( pdims.bottom - pdims.top )
                               , .left=( static_cast< uint32_t >( ( ( pdims.right - pdims.left ) * percs.first ) ) )
                               , .right=( static_cast< uint32_t >( ( pdims.right - pdims.left ) * percs.second ) ) };
                break;
            }
        }
    }

    // io::print( "dimensions|{}: {}\n", kmap_.fetch_heading( target ).value(), rv.value() );

    return rv;
}

auto Canvas::fetch_parent_pane( Uuid const& node )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto parent = KMAP_TRY( kmap_.fetch_parent( node ) );

    if( is_pane( parent ) )
    {
        rv = parent;
    }
    else
    {
        while( !is_pane( parent ) )
        {
            parent = KMAP_TRY( kmap_.fetch_parent( parent ) );
            
            if( is_pane( parent ) )
            {
                rv = parent;
            }
        }
    }

    return rv;
}

auto Canvas::fetch_subdivisions( Uuid const& pane )
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( view::make( pane ) | view::desc( "subdivision" ) | view::exists( kmap_ ) );
        })
    ;

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    rv = view::make( pane )
       | view::child( "subdivision" )
       | view::child
       | view::to_node_set( kmap_ );

    return rv;
}

auto Canvas::focus( Uuid const& pane )
    -> Result< void >
{
    using emscripten::val;
    
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( js::eval_void( io::format( "document.getElementById( '{}' ).focus();", pane ) ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::expand_path( std::string const& path )
    -> std::string
{
    return path
         | views::split( '.' )
         | views::join( std::string{ ".subdivision." } )
         | to< std::string >();
}

auto Canvas::fetch_pane( std::string const& path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const win_root = KMAP_TRY( fetch_window_root( kmap_ ) );
    auto const expanded = expand_path( path );
    auto const desc = KMAP_TRY( kmap_.fetch_descendant( win_root, win_root, io::format( "{}", expanded ) ) );

    if( is_pane( desc ) )
    {
        rv = desc;
    }
    
    return rv;
}

auto Canvas::breadcrumb_pane() const
    -> Uuid
{
    return breadcrumb_uuid;
}

auto Canvas::breadcrumb_fill_pane() const
    -> Uuid
{
    return breadcrumb_fill_uuid;
}

auto Canvas::breadcrumb_table_pane() const
    -> Uuid
{
    return breadcrumb_table_uuid;
}

auto Canvas::completion_overlay() const
    -> Uuid
{
    return completion_overlay_uuid;
}

auto Canvas::canvas_pane() const
    -> Uuid
{
    return util_canvas_uuid;
}

auto Canvas::cli_pane() const
    -> Uuid
{
    return cli_uuid;
}

auto Canvas::editor_pane() const
    -> Uuid
{
    return editor_uuid;
}

auto Canvas::network_pane() const
    -> Uuid
{
    return network_uuid;
}

auto Canvas::preview_pane() const
    -> Uuid
{
    return preview_uuid;
}

auto Canvas::text_area_pane() const
    -> Uuid
{
    return text_area_uuid;
}

auto Canvas::workspace_pane() const
    -> Uuid
{
    return workspace_uuid;
}

} // namespace kmap