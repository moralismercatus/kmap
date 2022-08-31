/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "canvas.hpp"

#include "com/database/root_node.hpp"
#include "com/event/event.hpp"
#include "com/network/network.hpp"
#include "com/option/option.hpp"
#include "component.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "error/network.hpp"
#include "error/node_manip.hpp"
#include "js_iface.hpp"
#include "path.hpp"
#include "path/act/abs_path.hpp"
#include "path/act/order.hpp"
#include "path/act/update_body.hpp"
#include "path/node_view.hpp"
#include "util/macro.hpp"
#include "util/window.hpp"

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

namespace kmap::com {

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

auto fetch_overlay_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "meta.setting.window.canvas.overlay";

    return view::make( abs_root )
         | view::direct_desc( root_path )
         | view::fetch_or_create_node( kmap );
}

auto fetch_overlay_root( Kmap const& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();

    return view::make( abs_root )
         | view::direct_desc( "meta.setting.window.canvas.overlay" )
         | view::fetch_node( kmap );
}

auto fetch_subdiv_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "meta.setting.window.canvas.subdivision";

    return view::make( abs_root )
         | view::direct_desc( root_path )
         | view::fetch_or_create_node( kmap );
}

auto fetch_window_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "meta.setting.window";

    return view::make( abs_root )
         | view::direct_desc( root_path )
         | view::fetch_or_create_node( kmap );
}

Canvas::Canvas( Kmap& kmap
              , std::set< std::string > const& requisites
              , std::string const& description )
    : Component{ kmap, requisites, description }
{
}

Canvas::~Canvas()
{
    try
    {
        for( auto const& elem_id : canvas_element_stack_
                                 | ranges::views::reverse )
        {
            KTRYE( js::erase_child_element( to_string( elem_id ) ) );
        }

        auto const& km = kmap_inst();
        auto const nw = KTRYE( fetch_component< com::Network >() );

        if( auto const croot = view::abs_root // Not using fetch_canvas_root(), because don't want to create if not already made.
                             | view::direct_desc( "meta.setting.window.canvas" )
                             | view::fetch_node( km )
          ; croot )
        {
            KTRYE( nw->erase_node( croot.value() ) );
        }
    }
    catch( std::exception& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto Canvas::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( initialize_root() );
    KTRY( initialize_panes() );
    KTRY( initialize_overlays() );
    KTRY( install_options() );
    KTRY( hide( editor_pane() ) );

    KTRY( update_overlays() );

    rv = outcome::success();

    return rv;
}

auto Canvas::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( update_all_panes() );
    KTRY( update_overlays() );

    KTRY( install_events() );

    // TODO: Not sure this is the best way to achieve this. Basically, at this point, Network is already loaded, but Canvas still needs to render current selected node.
    //       Perhaps a Network.canvas component needed?
    auto const nw = KTRY( fetch_component< com::Network >() );

    KTRY( nw->select_node( nw->root_node() ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::fetch_canvas_root()
    -> Result< Uuid >
{
    auto& km = kmap_inst();

    return KTRY( view::abs_root
               | view::direct_desc( "meta.setting.window.canvas" )
               | view::fetch_or_create_node( km ) );
}

auto Canvas::fetch_canvas_root() const
    -> Result< Uuid >
{
    auto const& km = kmap_inst();

    return KTRY( view::abs_root
               | view::direct_desc( "meta.setting.window.canvas" )
               | view::fetch_node( km ) );
}

// auto Canvas::init_event_callbacks()
//     -> Result< void >
// {
//     return js::eval_void( "window.onresize = function(){ kmap.canvas().update_all_panes(); };" );
// }

auto Canvas::install_events()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( fetch_component< com::EventStore >() );

    KTRY( estore->install_subject( "window" ) );
    KTRY( estore->install_verb( "scaled" ) );

    // window.onresize
    {
        window_events_.emplace_back( js::ScopedCode{ "window.onresize = function(){ kmap.event_store().fire_event( to_VectorString( [ 'subject.window', 'verb.scaled' ] ) ); };"
                                                   , "window.onresize = null;" } );
    }
    // window.onkeydown
    {
        // TODO: This all needs to change to dispatching events, not direct cli calls...
        auto const ctor =
R"%%%(
window.onkeydown = function( e )
{
    try
    {
        let key = e.keyCode ? e.keyCode : e.which;
        let is_shift = !!e.shiftKey;
        let cli = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );
        let editor = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

        switch ( key )
        {
            case 186: // colon
            {
                if( is_shift
                && cli != document.activeElement
                && editor != document.activeElement )
                {
                    clear_cli_error();
                    cli.value = "";
                    kmap.cli().focus();
                }
                break;
            }
            case 50: // '2' for '@'
            {
                if( is_shift
                && cli != document.activeElement
                && editor != document.activeElement )
                {
                    clear_cli_error();
                    cli.value = ":";
                    kmap.cli().focus();
                }
                break;
            }
            case 191: // forward slash
            {
                if( cli != document.activeElement
                && editor != document.activeElement )
                {
                    clear_cli_error();
                    cli.value = ":";
                    kmap.cli().focus();
                }
                break;
            }
            case 13: // enter
            {
                break;
            }
        }
    }
    catch( err )
    {
        console.log( String( err ) );
    }
};
)%%%";
        auto const dtor = "window.onkeydown = null;";
        window_events_.emplace_back( js::ScopedCode{ ctor
                                                   , dtor } );
    }

    #if 0

    KTRY( estore->install_subject( "keyboard.key.c" ) );
    KTRY( estore->install_verb( "depressed" ) );
    KTRY( estore->install_object( "network" ) );
    KTRY( estore->install_listener( "network.travel_left"
                                 , event::any_of{ event::action{ "keyboard.key.down" } }
                                 , event::all_of{ event::any_of{ event::object{ "keyboard.h" } }
                                                , event::excl{ event::state{ "keyboard", "keyboard.key.down" } } } );
    KTRY( estore->install_listener( "network.travel_left"
                                 , event::any_of{ event::action{ "keyboard.key.down" } }
                                 , event::all_of{ event::any_of{ event::object{ "keyboard.h" } }
                                                , event::excl{ event::state{ "keyboard", "keyboard.key.down" } } } );
                                                // , event::none_of{ event::state{ "keyboard.ctrl", "keyboard.key.down" }
                                                //                 , event::state{ "keyboard.alt", "keyboard.key.down" } ) );
    KTRY( estore->install_listener( "network.travel_left"
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
    auto const opt = KTRY( fetch_component< com::OptionStore >() );

    // Network
    KTRY( opt->install_option( "canvas.network.background.color"
                            , "Sets the background color for the nextwork pane."
                            , "\"#222222\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().network_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    // Preview
    KTRY( opt->install_option( "canvas.preview.background.color"
                            , "Sets the background color for the preview pane."
                            , "\"#222222\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KTRY( opt->install_option( "canvas.preview.text.color"
                            , "Sets the text color for the preview pane."
                            , "\"white\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.color = option_value;" ) );
    KTRY( opt->install_option( "canvas.preview.text.size"
                            , "Text size."
                            , "\"large\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.fontSize = option_value;" ) );
    KTRY( opt->install_option( "canvas.preview.scrollbar"
                            , "Specify scroll bar."
                            , "\"auto\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.overflow = option_value;" ) );
    KTRY( opt->install_option( "canvas.preview.whitespace_wrap"
                            , "Specify scroll behavior."
                            , "\"nowrap\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value_or_throw() ).style.whiteSpace = option_value;" ) );
    // Editor
    KTRY( opt->install_option( "canvas.editor.background.color"
                            , "Sets the background color for the editor pane."
                            , "\"#222222\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KTRY( opt->install_option( "canvas.editor.text.color"
                            , "Sets the text color for the editor pane."
                            , "\"white\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.color = option_value;" ) );
    KTRY( opt->install_option( "canvas.editor.text.size"
                            , "Text size."
                            , "\"large\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.fontSize = option_value;" ) );
    KTRY( opt->install_option( "canvas.editor.scrollbar"
                            , "Specify scroll behavior."
                            , "\"auto\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.overflow = option_value;" ) );
    KTRY( opt->install_option( "canvas.editor.whitespace_wrap"
                            , "Specify scroll behavior."
                            , "\"nowrap\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value_or_throw() ).style.whiteSpace = option_value;" ) );
    // CLI
    KTRY( opt->install_option( "canvas.cli.background.color"
                            , "Sets the background color for the cli pane."
                            , "\"#222222\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KTRY( opt->install_option( "canvas.cli.text.color"
                            , "Sets the text color for the cli pane."
                            , "\"white\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value_or_throw() ).style.color = option_value;" ) );
    KTRY( opt->install_option( "canvas.cli.text.size"
                            , "Text size."
                            , "\"large\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value_or_throw() ).style.fontSize = option_value;" ) );
    // Completion Box
    KTRY( opt->install_option( "canvas.completion_box.background.color"
                            , "Sets the background color for the completion box."
                            , "\"#333333\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.backgroundColor = option_value;" ) );
    KTRY( opt->install_option( "canvas.completion_box.text.color"
                            , "Sets the text color for the completion box popup."
                            , "\"white\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.color = option_value;" ) );
    KTRY( opt->install_option( "canvas.completion_box.padding"
                            , "Sets the padding between edge of box and internal text."
                            , "\"0px\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.padding = option_value;" ) );
    KTRY( opt->install_option( "canvas.completion_box.border.radius"
                            , "Sets the rounding radius for the corners of the box."
                            , "\"5px\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.borderRadius = option_value;" ) );
    KTRY( opt->install_option( "canvas.completion_box.border.style"
                            , "Border style"
                            , "\"outset\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.borderStyle = option_value;" ) );
    KTRY( opt->install_option( "canvas.completion_box.border.width"
                            , "Width of border."
                            , "\"thin\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.borderWidth = option_value;" ) );
    KTRY( opt->install_option( "canvas.completion_box.scrollbar"
                            , "Specify scroll behavior."
                            , "\"auto\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.overflow = option_value;" ) );
    // I think all canvas items are absolute... I think this gets encoded when they are created. Probably doesn't belong here.
    KTRY( opt->install_option( "canvas.completion_box.position_type"
                            , "Sets the rounding radius for the corners of the box."
                            , "\"absolute\""
                            , "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ).value_or_throw() ).style.position = option_value;" ) );
    KTRY( opt->install_option( "canvas.completion_box.placement_order"
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
        auto const& km = kmap_inst();
        if( auto const completed = complete_path_reducing( km
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
    auto const& km = kmap_inst();
    auto const nw = KTRYE( fetch_component< com::Network >() );

    if( auto const overlay_root = fetch_overlay_root( km )
      ; overlay_root )
    {
        rv = is_ancestor( *nw, overlay_root.value(), node );
    }

    return rv;
}

auto Canvas::is_pane( Uuid const& node ) const
    -> bool
{
    using emscripten::val;

    auto rv = false;
    auto const nw = KTRYE( fetch_component< com::Network >() );

    if( auto const canvas_root = fetch_canvas_root()
      ; canvas_root )
    {
        if( auto const parent = nw->fetch_parent( node )
          ; parent )
        {
            rv = nw->is_lineal( canvas_root.value(), node )
              && ( canvas_root.value() == node || KTRYE( nw->fetch_heading( parent.value() ) ) == "subdivision" )
              && nw->is_child( node
                             , "orientation"
                             , "base"
                             , "hidden"
                             , "type"
                             , "subdivision" );
            //   && js::fetch_element_by_id< val >( to_string( node ) ).has_value();
        }
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
    KTRY( delete_subdivisions( breadcrumb_table_pane() ) );

    io::print( "bc.size(): {}\n", bc.size() );

    auto accumulated_width = decltype( Division::base ){};

    io::print( "titles.size(): {}\n", bc.size() );

    // TODO:
    // There's a major problem here. Namely, that create_child will be called, and not just once, but repeatedly.
    // This will not only invalidate the DB cache, but also require multiple SQL writes (via delete child / create child)
    // So... what's to be done?
    // In a way, I don't want these to even be saved to the DB.
    auto const nw = KTRY( fetch_component< com::Network >() );

    for( auto const& [ index, target ] : views::enumerate( bc ) )
    {
        auto const title = KTRY( nw->fetch_title( target ) );
        io::print( "creating subdivision: {}\n", index );
        // Determine width. Accumulate widths to determine base of fill div.
        auto const subdiv = KTRY( subdivide( breadcrumb_table_pane()
                                               , fmt::format( "{}", index ) // Simply use enumeration value as pane heading.
                                               , Division{ Orientation::horizontal, accumulated_width, false, "div" } ) );
        io::print( "created subdivision: {}, {}\n", index, to_string( subdiv ) );
        auto div = KTRY( js::fetch_element_by_id< val >( to_string( subdiv ) ) );

        div.set( "innerText", fmt::format( "{}", title ) );
        div.set( "style", fmt::format( "color: black", title ) );
        div.set( "onclick", fmt::format( "function(){{ kmap.select_node( '{}' ).throw_on_error(); }}", to_string( target ) ) );
        accumulated_width += 0.10;//calc_text_width( title );
    }

    // TODO: fill remaining space with fill div, based at accumulated_width.
    KTRY( update_pane_descending( breadcrumb_pane() ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::subdivide( Uuid const& parent_pane
                      , Uuid const& pane_id
                      , Heading const& heading
                      , Division const& subdiv )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_pane( parent_pane ), error_code::canvas::invalid_subdiv );
    KMAP_ENSURE( is_valid_heading( heading ), error_code::network::invalid_heading );

    auto const parent_pane_subdivn = KTRY( nw->fetch_child( parent_pane, "subdivision" ) );

    rv = create_subdivision( parent_pane_subdivn
                           , pane_id
                           , heading
                           , subdiv );

    return rv;
}

auto Canvas::subdivide( Uuid const& parent_pane
                      , Heading const& heading
                      , Division const& subdiv )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( is_pane( parent_pane ), error_code::canvas::invalid_subdiv );
    KMAP_ENSURE( is_valid_heading( heading ), error_code::network::invalid_heading );

    auto& km = kmap_inst();
    auto const parent_pane_subdivn = KTRY( view::make( parent_pane )
                                         | view::child( "subdivision" )
                                         | view::fetch_node( km ) );

    rv = create_subdivision( parent_pane_subdivn
                           , heading
                           , subdiv );

    return rv;
}

auto Canvas::update_overlays()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const& km = kmap_inst();
    auto const canvas_root = KTRY( fetch_canvas_root() );

    for( auto const overlays = view::make( canvas_root )
                             | view::child( "overlay" )
                             | view::child
                             | view::to_node_set( km )
       ; auto const& overlay : overlays )
    {
        if( !js::exists( overlay ) )
        {
            KTRY( create_html_child_element( "div", canvas_root, overlay ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::update_all_panes()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const canvas_root = KTRY( static_cast< Canvas const& >( *this ).fetch_canvas_root() );

    KTRY( update_pane( canvas_root ) );

    rv = outcome::success();

    return rv;
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
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const subdivn = KTRY( pane_subdivision( root ) );
    
    KMAP_ENSURE( is_pane( root ), error_code::network::invalid_node );

    // TODO: I think if I can check for the existence of the element, and create it if it doesn't exist, then I can load.
    //       The one piece I'm missing - I think - is the type of element ('text_area', 'div', etc.). If I have this info,
    //       No problem!
    // html element creation must happen before subdiv, as subdiv depends on parent element.
    if( !js::exists( root ) )
    {
        auto const canvas_root = KTRY( static_cast< Canvas const& >( *this ).fetch_canvas_root() );

        if( root == canvas_root )
        {
            KTRY( create_html_root_element( canvas_root ) );
        }
        else
        {
            auto const parent_pane = KTRY( fetch_parent_pane( root ) );
            auto const typen = KTRY( view::make( root ) | view::child( "type" ) | view::fetch_node( km ) );
            auto const elem_type = KTRY( nw->fetch_body( typen ) );
            KTRY( create_html_child_element( elem_type, parent_pane, root ) );

        }

    }

    for( auto const& subdiv : nw->fetch_children( subdivn ) )
    {
        KTRY( update_pane_descending( subdiv ) );
    }

    // io::print( "updating pane: {}\n", kmap_.absolute_path_flat( root ) );

    {
        auto style = KTRY( js::eval< val >( io::format( "return document.getElementById( '{}' ).style;", root ) ) ); 

        auto const dims = KTRY( dimensions( root ) );
        // io::print( "'{}' dims: {}\n", KMAP_TRYE( nw->fetch_heading( root ) ), dims );
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
    }

    {
        auto const hidden_node = KTRY( nw->fetch_child( root, "hidden" ) );
        auto const hidden_body = KTRY( nw->fetch_body( hidden_node ) );

        KTRY( js::eval_void( io::format( "document.getElementById( '{}' ).hidden={};", to_string( root ), hidden_body ) ) );
    }

    rv = outcome::success();

    return rv;
}

// TODO: What's diff between this and update_all_panes()?
// TODO: Ensure JS calls to this are debounced. No reason not to, I think.
auto Canvas::update_panes()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const canvas_root = KTRY( static_cast< Canvas const& >( *this ).fetch_canvas_root() );

    KMAP_ENSURE( is_pane( canvas_root ), error_code::network::invalid_node );

    KTRY( update_pane_descending( canvas_root ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::rebase_internal( Uuid const& pane
                            , float const base )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_pane( pane ), error_code::network::invalid_node );

    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto const basen = KTRY( view::make( pane )
                           | view::child( "base" )
                           | view::fetch_node( km ) );

    KTRY( nw->update_body( basen, io::format( "{:.4f}", base ) ) );

    // Ensure network displays them as ordered according to their position.
    {
        auto const parent = KTRY( nw->fetch_parent( pane ) );
        auto const children = view::make( parent )
                            | view::child
                            | view::to_node_set( km )
                            | act::order( km )
                            | actions::sort( [ & ]( auto const& lhs, auto const& rhs ){ return pane_base( lhs ).value() < pane_base( rhs ).value(); } );
        KTRY( nw->reorder_children( parent, children ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::rebase( Uuid const& pane
                   , float const base )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( rebase_internal( pane, base ) );
    KTRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reorient_internal( Uuid const& pane
                              , Orientation const& orientation )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    KTRY( view::make( pane )
        | view::child( "orientation" )
        | act::update_body( km, to_string( orientation ) ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::orient( Uuid const& pane
                   , Orientation const& orientation )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( reorient_internal( pane, orientation ) );
    KTRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reorient( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const prev_orient = KTRY( pane_orientation( pane ) );
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

    KTRY( hide_internal( pane, true ) );
    KTRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::hide_internal( Uuid const& pane
                          , bool const hidden )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE_MSG( nw->exists( pane ), error_code::network::invalid_node, to_string_elaborated( km, pane ) );
    KMAP_ENSURE_MSG( is_pane( pane ), error_code::network::invalid_node, to_string_elaborated( km, pane ) );

    auto const hiddenn = KTRY( view::make( pane )
                             | view::child( "hidden" )
                             | view::fetch_node( km ) );
    auto const hidden_str = kmap::to_string( hidden );

    KTRY( nw->update_body( hiddenn, hidden_str ) );

    rv = outcome::success();

    return rv;
}

// TODO: I don't think this is even used... it is created and destroyed, but all panes use 'div', 'textarea', etc. attached to 'body'.
//       Bypassing this. I think this may have been a vestige of another windowing approach. 
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

auto Canvas::initialize_panes()
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( create_html_canvas( util_canvas_uuid ) );

    auto const canvas = KTRY( fetch_canvas_root() );
                            //KTRY( reset_breadcrumb( canvas ) );
    auto const workspace  = KTRY( subdivide( canvas, workspace_pane(), "workspace", Division{ Orientation::vertical, 0.025f, false, "div" } ) );
                            KTRY( subdivide( workspace, network_pane(), "network", Division{ Orientation::horizontal, 0.000f, false, "div" } ) );
    auto const text_area  = KTRY( subdivide( workspace, text_area_pane(), "text_area", Division{ Orientation::horizontal, 0.660f, false, "div" } ) );
                            KTRY( subdivide( text_area, editor_pane(), "editor", Division{ Orientation::horizontal, 0.000f, true, "textarea" } ) );
                            KTRY( subdivide( text_area, preview_pane(), "preview", Division{ Orientation::horizontal, 1.000f, false, "div" } ) );
                            KTRY( subdivide( canvas, cli_pane(), "cli", Division{ Orientation::horizontal, 0.975f, false, "input" } ) );

    KTRY( update_panes() );
    // KTRY( init_event_callbacks() );
    KTRY( install_events() );

    rv = outcome::success();

    return rv;
}

auto Canvas::initialize_overlays()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( create_overlay( completion_overlay(), "completion_overlay"/*, "div"*/ ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reset_breadcrumb( Uuid const& supdiv )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( is_pane( supdiv ), error_code::canvas::invalid_pane );

    /*auto const breadcrumb = */KTRY( subdivide( supdiv, breadcrumb_pane(), "breadcrumb", Division{ Orientation::horizontal, 0.000f, false, "div" } ) );

    // How many cells do we allow? 10? To a certain extent, it depends on screen size.
    // For now, 10 sounds reasonable.
    // for( auto const& i : views::indices( 10 ) )
    // {
    //     KTRY( subdivide( breadcrumb, breadcrumb_table_pane(), fmt::format( "{}", i ), Division{ Orientation::vertical, 0.000f, true }, "div" ) );
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

    auto& km = kmap_inst();
    auto const vsub = view::make( target );

    KTRY( vsub | view::child( "orientation" ) | view::fetch_or_create_node( km ) );
    KTRY( vsub | view::child( "base" ) | view::fetch_or_create_node( km ) );
    KTRY( vsub | view::child( "hidden" ) | view::fetch_or_create_node( km ) );
    KTRY( vsub | view::child( "type" ) | view::fetch_or_create_node( km ) );
    KTRY( vsub | view::child( "subdivision" ) | view::fetch_or_create_node( km ) );

    KTRY( vsub | view::child( "orientation" ) | act::update_body( km, to_string( subdiv.orientation ) ) );
    KTRY( vsub | view::child( "base" ) | act::update_body( km, io::format( "{:.4f}", subdiv.base ) ) );
    KTRY( vsub | view::child( "hidden" ) | act::update_body( km, kmap::to_string( subdiv.hidden ) ) );
    KTRY( vsub | view::child( "type" ) | act::update_body( km, subdiv.elem_type ) );

    rv = outcome::success();

    return rv;
}

// TODO: So, need to have a load variety here. Or maybe create the child, then a separate routine that reads overlays, checks for their existence, and creates
//       html element if needed, for loading purposes. That seems a better option, more consistent with the panes approach.
auto Canvas::create_overlay( Uuid const& id
                           , std::string const& heading )
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

    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    // auto const canvas_root = KTRY( fetch_canvas_root() );
    auto const overlay_root = KTRY( fetch_overlay_root( km ) );
    auto const overlay = KTRY( nw->create_child( overlay_root, id, heading ) );

    rv = overlay;

    return rv;
}

auto Canvas::create_subdivision( Uuid const& parent
                               , Uuid const& child
                               , std::string const& heading
                               , Division const& subdiv )
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

    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const subdivn = KTRY( nw->create_child( parent, child, heading ) );

    KTRY( make_subdivision( subdivn, subdiv ) );

    rv = subdivn;

    return rv;
}

auto Canvas::create_subdivision( Uuid const& parent
                               , std::string const& heading
                               , Division const& subdiv )
    -> Result< Uuid >
{
    return create_subdivision( parent 
                             , gen_uuid()
                             , heading
                             , subdiv );
}

auto Canvas::create_html_root_element( Uuid const& root_pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_ASSERT( !js::element_exists( to_string( root_pane ) ) );

    // TODO: Shouldn't... this be a child of the 'canvas' element that we created?
    //       Unless... we don't actually use the 'canvas' type, but rather divs only.
    //       And... what is the 'canvas' element's purpose then? Anyway, 'div's will simplify things, as I'm familiar with those.
    KTRY( js::eval_void( io::format( "let canvas_div = document.createElement( 'div' );"
                                     "canvas_div.id = '{}';"
                                     "let body_tag = document.getElementsByTagName( 'body' )[ 0 ];"
                                     "body_tag.appendChild( canvas_div );" 
                                    , to_string( root_pane ) ) ) );

    canvas_element_stack_.emplace_back( root_pane );

    rv = outcome::success();

    return rv;
}

auto Canvas::initialize_root()
    -> Result< Uuid >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const canvas_root = KTRY( fetch_canvas_root() );

    KTRY( make_subdivision( canvas_root, { Orientation::horizontal, 0.0f, false } ) );

    rv = canvas_root;

    fmt::print( "finished: Canvas::initialize_root\n" );

    return rv;
}

auto Canvas::reveal( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( hide_internal( pane, false ) );
    KTRY( update_pane( pane ) );

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
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_pane( subdiv ), error_code::network::invalid_node );

    auto const orient_node = KTRY( nw->fetch_child( subdiv, "orientation" ) );
    auto const orient_body = KTRY( nw->fetch_body( orient_node ) );

    rv = from_string< Orientation >( orient_body );

    return rv;
}

auto Canvas::pane_base( Uuid const& subdiv )
    -> Result< float >
{
    auto rv = KMAP_MAKE_RESULT( float );

    KMAP_ENSURE( is_pane( subdiv ), error_code::network::invalid_node );

    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const base_node = KTRY( nw->fetch_child( subdiv, "base" ) );
    auto const base_body = KTRY( nw->fetch_body( base_node ) );
    
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
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_pane( pane ), error_code::network::invalid_node );

    auto const hidden_node = KTRY( nw->fetch_child( pane, "hidden" ) );
    auto const hidden_body = KTRY( nw->fetch_body( hidden_node ) );

    rv = KTRY( from_string< bool >( hidden_body ) );

    return rv;
}

auto Canvas::pane_subdivision( Uuid const& subdiv )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    KMAP_ENSURE_MSG( is_pane( subdiv ), error_code::canvas::invalid_pane, nw->fetch_heading( subdiv ).value() );

    rv = nw->fetch_child( subdiv, "subdivision" );
    
    return rv;
}

auto Canvas::pane_path( Uuid const& subdiv )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const canvas_root = KTRY( fetch_canvas_root() );
    auto const ids = KTRY( view::make( canvas_root )
                         | view::desc( subdiv )
                         | act::abs_path( km ) );

    KMAP_ENSURE( is_pane( subdiv ), error_code::network::invalid_node );

    auto const headings = ids
                        | views::stride( 2 ) // Drop "subdivision" from path.
                        | views::transform( [ & ]( auto const& e ){ return nw->fetch_heading( e ).value(); } )
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
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );
    KMAP_ENSURE( js::element_exists( to_string( pane ) ), error_code::canvas::invalid_pane );
    
    for( auto const& sp : KTRY( fetch_subdivisions( pane ) ) )
    {
        KTRY( delete_pane( sp ) );
    }

    KTRY( js::eval_void( fmt::format( "document.getElementById( '{}' ).remove();", to_string( pane ) ) ) );
    KTRY( nw->erase_node( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::delete_subdivisions( Uuid const& pane )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    for( auto const children = view::make( pane )
                             | view::child( "subdivision" )
                             | view::child
                             | view::to_node_set( km )
        ; auto const& child : children )
    {
        KTRY( delete_pane( child ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::dimensions( Uuid const& target )
    -> Result< Dimensions >
{
    auto rv = KMAP_MAKE_RESULT( Dimensions );
    auto const canvas_root = KTRY( fetch_canvas_root() ); 
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( nw->is_lineal( canvas_root, target ), error_code::network::invalid_lineage );

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
        auto const parent_pane = KTRY( fetch_parent_pane( target ) );
        auto const orientation = KTRY( pane_orientation( parent_pane ) );
        auto const siblings = view::make( target )
                            | view::parent
                            | view::child
                            | view::to_node_set( km )
                            | act::order( km );
        auto const percs = compute_percents( siblings );
        auto const pdims = KTRY( dimensions( parent_pane ) );

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

    // io::print( "dimensions|{}: {}\n", nw->fetch_heading( target ).value(), rv.value() );

    return rv;
}

auto Canvas::fetch_parent_pane( Uuid const& node )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto parent = KTRY( nw->fetch_parent( node ) );

    if( is_pane( parent ) )
    {
        rv = parent;
    }
    else
    {
        while( !is_pane( parent ) )
        {
            parent = KTRY( nw->fetch_parent( parent ) );
            
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
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( view::make( pane ) | view::desc( "subdivision" ) | view::exists( km ) );
        })
    ;

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    rv = view::make( pane )
       | view::child( "subdivision" )
       | view::child
       | view::to_node_set( km );

    return rv;
}

auto Canvas::focus( Uuid const& pane )
    -> Result< void >
{
    using emscripten::val;
    
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( js::eval_void( io::format( "document.getElementById( '{}' ).focus();", pane ) ) );

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
    auto& km = kmap_inst();
    auto const win_root = KTRY( fetch_window_root( km ) );
    auto const expanded = expand_path( path );
    auto const desc = KTRY( view::make( win_root )
                          | view::desc( io::format( "{}", expanded ) )
                          | view::fetch_node( km ) );

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

namespace binding {

using namespace emscripten;

struct Canvas
{
    kmap::Kmap& km;

    Canvas( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto complete_path( std::string const& path ) const
        -> StringVec
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->complete_path( path );
    }

    auto fetch_base( Uuid const& pane ) const
        -> Result< float >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->pane_base( pane );
    }

    auto fetch_orientation( Uuid const& pane ) const
        -> Result< kmap::com::Orientation >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->pane_orientation( pane );
    }

    auto fetch_pane( std::string const& path ) const
        -> kmap::binding::Result< Uuid >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->fetch_pane( path );
    }
    
    auto focus( Uuid const& pane )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->focus( pane );
    }

    auto hide( Uuid const& pane )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->hide( pane );
    }

    auto orient( Uuid const& pane
               , kmap::com::Orientation const orientation )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->orient( pane, orientation );
    }

    auto rebase( Uuid const& pane
               , float const base )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->rebase( pane, base );
    }

    auto reorient( Uuid const& pane )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->reorient( pane );
    }

    auto reveal( Uuid const& pane )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->reveal( pane );
    }

    auto subdivide( Uuid const& pane
                  , std::string const& heading
                  , std::string const& orientation
                  , float const base
                  , std::string const& elem_type )
        -> kmap::binding::Result< Uuid >
    {
        auto const parsed_orient = KTRY( from_string< Orientation >( orientation ) );

        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->subdivide( pane, heading, Division{ parsed_orient, base, false, elem_type } );
    }

    auto update_all_panes()
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->update_all_panes();
    }

    auto breadcrumb_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->breadcrumb_pane(); } 
    auto breadcrumb_table_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->breadcrumb_table_pane(); } 
    auto canvas_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->canvas_pane(); } 
    auto cli_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->cli_pane(); } 
    auto completion_overlay() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->completion_overlay(); }
    auto editor_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->editor_pane(); } 
    auto network_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->network_pane(); }
    auto preview_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->preview_pane(); }
    auto text_area_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->text_area_pane(); }
    auto workspace_pane() const -> Uuid { return KTRYE( km.fetch_component< kmap::com::Canvas >() )->workspace_pane(); }
};

auto canvas()
    -> binding::Canvas
{
    return binding::Canvas{ kmap::Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_canvas )
{
    function( "canvas", &kmap::com::binding::canvas );
    class_< kmap::com::binding::Canvas >( "Canvas" )
        .function( "breadcrumb_pane", &kmap::com::binding::Canvas::breadcrumb_pane )
        .function( "breadcrumb_table_pane", &kmap::com::binding::Canvas::breadcrumb_table_pane )
        .function( "canvas_pane", &kmap::com::binding::Canvas::canvas_pane )
        .function( "cli_pane", &kmap::com::binding::Canvas::cli_pane )
        .function( "complete_path", &kmap::com::binding::Canvas::complete_path )
        .function( "completion_overlay", &kmap::com::binding::Canvas::completion_overlay )
        .function( "editor_pane", &kmap::com::binding::Canvas::editor_pane )
        .function( "fetch_base", &kmap::com::binding::Canvas::fetch_base )
        .function( "fetch_orientation", &kmap::com::binding::Canvas::fetch_orientation )
        .function( "fetch_pane", &kmap::com::binding::Canvas::fetch_pane )
        .function( "focus", &kmap::com::binding::Canvas::focus )
        .function( "hide", &kmap::com::binding::Canvas::hide )
        .function( "network_pane", &kmap::com::binding::Canvas::network_pane )
        .function( "orient", &kmap::com::binding::Canvas::orient )
        .function( "preview_pane", &kmap::com::binding::Canvas::preview_pane )
        .function( "rebase", &kmap::com::binding::Canvas::rebase )
        .function( "reorient", &kmap::com::binding::Canvas::reorient )
        .function( "reveal", &kmap::com::binding::Canvas::reveal )
        .function( "text_area_pane", &kmap::com::binding::Canvas::text_area_pane )
        .function( "update_all_panes", &kmap::com::binding::Canvas::update_all_panes )
        .function( "workspace_pane", &kmap::com::binding::Canvas::workspace_pane )
        ;
    KMAP_BIND_RESULT( kmap::com::Orientation );
    enum_< kmap::com::Orientation >( "Orientation" )
        .value( "horizontal", kmap::com::Orientation::horizontal )
        .value( "vertical", kmap::com::Orientation::vertical )
        ;
}

} // namespace binding

namespace {
namespace canvas_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Canvas
,   std::set({ "command_store"s, "event_store"s, "option_store"s, "network"s, "root_node"s })
,   "canvas related functionality"
);

} // namespace canvas_def 
}

} // namespace kmap::com

namespace kmap
{
    template<>
    auto from_string( std::string const& s )
        -> Result< com::Orientation >
    {
        auto rv = KMAP_MAKE_RESULT_EC( com::Orientation, error_code::common::conversion_failed );

        if( s == "horizontal" )
        {
            rv = com::Orientation::horizontal;
        }
        else if( s == "vertical" )
        {
            rv = com::Orientation::vertical;
        }

        return rv;
    }
}