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
#include "util/window.hpp"
#include "utility.hpp"
#include "js_iface.hpp"

#include <emscripten.h>
#include <emscripten/val.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/stride.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/replace.hpp>

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

auto set_fwd_breadcrumb( std::string_view const text )
    -> bool
{
    using emscripten::val;

    auto rv = bool{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( js::fetch_element_by_id< val >( to_string( breadcrumb_uuid ) ).has_value() );
        })
    ;

    if( auto fwd_breadcrumb = js::fetch_element_by_id< val >( to_string( breadcrumb_uuid ) )
      ; fwd_breadcrumb )
    {
        io::print( "settting breadcrumb: {}\n", text );
        fwd_breadcrumb.value().set( "innerHTML"
                                  , std::string{ text } );

        rv = true;
    }
    else
    {
        io::print( "failed to set breadcrumb\n" );
        rv = false;
    }

    return rv;
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

auto fetch_window_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "/meta.window";

    return kmap.fetch_or_create_descendant( abs_root, abs_root, root_path );
}

auto fetch_canvas_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "/meta.window.canvas";

    return kmap.fetch_or_create_descendant( abs_root, abs_root, root_path );
}

auto fetch_subdiv_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto const abs_root = kmap.root_node_id();
    auto const root_path = "/meta.window.canvas.subdivision";

    return kmap.fetch_or_create_descendant( abs_root, abs_root, root_path );
}

Canvas::Canvas( Kmap& kmap )
    : kmap_{ kmap }
{
}

auto Canvas::complete_path( std::string const& path )
    -> StringVec
{
    io::print( "complete_path.path: {}\n", path );
    auto rv = StringVec{};
    auto const canvas_root = fetch_canvas_root( kmap_ );
    if( canvas_root )
    {
        auto const inflated = std::regex_replace( path, std::regex{ "\\." }, ".subdivision." );
        io::print( "complete_path.inflated: {}\n", inflated );
        if( auto const completed = complete_path_reducing( kmap_ 
                                                         , canvas_root.value()
                                                         , canvas_root.value()
                                                         , inflated )
          ; completed )
        {
            for( auto const completed_value = completed.value()
               ; auto const& c : completed_value )
            {
                io::print( "complete_path.completed: {}\n", c.path );
                auto const deflated = std::regex_replace( c.path, std::regex{ "\\.subdivision\\." }, "." );
                io::print( "complete_path.deflated: {}\n", deflated );
                if( is_pane( c.target ) )
                {
                    rv.emplace_back( deflated );
                }
            }
        }
    }
    
    return rv;
}

auto Canvas::is_pane( Uuid const& node )
    -> bool
{
    auto rv = false;

    if( auto const canvas_root = fetch_canvas_root( kmap_ )
      ; canvas_root )
    {
        rv = canvas_root.value() == node
          || ( kmap_.is_lineal( canvas_root.value(), node )
            && kmap_.fetch_heading( kmap_.fetch_parent( node ).value() ).value() == "subdivision"
            && kmap_.is_child( node
                             , "orientation"
                             , "base"
                             , "hidden"
                             , "subdivision" ) );
    }

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

    KMAP_ENSURE( rv, is_pane( parent_pane ), error_code::canvas::invalid_subdiv );
    KMAP_ENSURE( rv, is_valid_heading( heading ), error_code::network::invalid_heading );

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

    KMAP_ENSURE( rv, is_pane( parent_pane ), error_code::canvas::invalid_subdiv );
    KMAP_ENSURE( rv, is_valid_heading( heading ), error_code::network::invalid_heading );

    auto const parent_pane_subdivn = kmap_.node_view( parent_pane )[ "/subdivision" ];

    rv = create_subdivision( parent_pane_subdivn
                           , heading
                           , subdiv
                           , elem_type );

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
    auto const subdivn = KMAP_TRY( pane_subdivision( root ) );

    KMAP_ENSURE( rv, is_pane( root ), error_code::network::invalid_node );

    auto style = KMAP_TRY( js::eval< val >( io::format( "document.getElementById( '{}' ).style;", root ) ) ); 

    auto const dims = KMAP_TRY( dimensions( root ) );
    // io::print( "{} dims: {}\n", kmap_.fetch_heading( root ).value(), dims );
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
    auto const canvas_root = KMAP_TRY( fetch_canvas_root( kmap_ ) );

    KMAP_ENSURE( rv, is_pane( canvas_root ), error_code::network::invalid_node );

    KMAP_TRY( update_pane_descending( canvas_root ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::rebase_internal( Uuid const& pane
                            , float const base )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( rv, is_pane( pane ), error_code::network::invalid_node );

    auto const basen = KMAP_TRY( kmap_.fetch_descendant( pane, pane, "/base" ) );

    kmap_.update_body( basen, io::format( "{:.4f}", base ) );

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

    KMAP_ENSURE( rv, is_pane( pane ), error_code::network::invalid_node );

    auto const orientn = KMAP_TRY( kmap_.fetch_descendant( pane, pane, "/orientation" ) );

    kmap_.update_body( orientn, to_string( orientation ) );

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

    KMAP_ENSURE( rv, is_pane( pane ), error_code::network::invalid_node );

    auto const hiddenn = KMAP_TRY( kmap_.fetch_descendant( pane, pane, "/hidden" ) );
    auto const hidden_str = to_string( hidden );

    kmap_.update_body( hiddenn, hidden_str );
    
    KMAP_TRY( js::eval_void( io::format( "document.getElementById( '{}' ).hidden={};", to_string( pane ), hidden_str ) ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reset()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const canvas     = KMAP_TRY( reset_root() );
                            KMAP_TRY( subdivide( canvas, breadcrumb_pane(), "breadcrumb", Division{ Orientation::horizontal, 0.000f, false }, "div" ) );
    auto const workspace  = KMAP_TRY( subdivide( canvas, workspace_pane(), "workspace", Division{ Orientation::vertical, 0.025f, false }, "div" ) );
                            KMAP_TRY( subdivide( workspace, network_pane(), "network", Division{ Orientation::horizontal, 0.000f, false }, "div" ) );
    auto const text_area  = KMAP_TRY( subdivide( workspace, text_area_pane(), "text_area", Division{ Orientation::horizontal, 0.330f, false }, "div" ) );
                            KMAP_TRY( subdivide( text_area, editor_pane(), "editor", Division{ Orientation::horizontal, 0.000f, true }, "textarea" ) );
                            KMAP_TRY( subdivide( text_area, preview_pane(), "preview", Division{ Orientation::horizontal, 1.000f, false }, "div" ) );
                            KMAP_TRY( subdivide( canvas, cli_pane(), "cli", Division{ Orientation::horizontal, 0.975f, false }, "input" ) );

    KMAP_TRY( update_panes() );

    rv = outcome::success();

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

    KMAP_TRY( kmap_.fetch_or_create_descendant( target, target, "/orientation" ) );
    KMAP_TRY( kmap_.fetch_or_create_descendant( target, target, "/base" ) );
    KMAP_TRY( kmap_.fetch_or_create_descendant( target, target, "/hidden" ) );
    KMAP_TRY( kmap_.fetch_or_create_descendant( target, target, "/subdivision" ) );
    KMAP_TRY( reorient_internal( target, subdiv.orientation ) );
    KMAP_TRY( rebase_internal( target, subdiv.base ) );
    KMAP_TRY( hide_internal( target, subdiv.hidden ) );
    KMAP_TRY( update_pane( target ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::create_subdivision( Uuid const& parent
                               , Uuid const& child
                               , std::string const& heading
                               , Division const& subdiv
                               , std::string const& elem_type )
    -> Result< Uuid >
{
    using emscripten::val;

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

    KMAP_TRY( js::eval_void( io::format( "let child_div = document.createElement( '{}' );"
                                         "child_div.id = '{}';"
                                         "let parent_div = document.getElementById( '{}' );"
                                         "parent_div.appendChild( child_div );" 
                                       , elem_type
                                       , to_string( subdivn )
                                       , to_string( parent_pane ) ) ) );
io::print( "create_subdiv.heading: {}, elem_type: {}\n", heading, elem_type );

    KMAP_TRY( make_subdivision( subdivn, subdiv ) );

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

auto Canvas::reset_root()
    -> Result< Uuid >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const canvas_root = KMAP_TRY( fetch_canvas_root( kmap_ ) );

    if( !js::fetch_element_by_id< val >( to_string( canvas_root ) ) )
    {
        KMAP_TRY( js::eval_void( io::format( "let canvas_div = document.createElement( 'div' );"
                                             "canvas_div.id = '{}';"
                                             "let body_tag = document.getElementsByTagName( 'body' )[ 0 ];"
                                             "body_tag.appendChild( canvas_div );" 
                                           , to_string( canvas_root ) ) ) );
    }

    KMAP_TRY( make_subdivision( canvas_root, { Orientation::horizontal, 0.0f, false } ) );

    rv = canvas_root;

    return rv;
}

auto Canvas::reveal( Uuid const& pane )
    -> Result< void >
{
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( hide_internal( pane, false ) );
    KMAP_TRY( update_pane( pane ) );

    rv = outcome::success();

    return rv;
}
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

    KMAP_ENSURE( rv, is_pane( subdiv ), error_code::network::invalid_node );

    auto const orient_node = KMAP_TRY( kmap_.fetch_child( subdiv, "orientation" ) );
    auto const orient_body = KMAP_TRY( kmap_.fetch_body( orient_node ) );

    rv = from_string< Orientation >( orient_body );

    return rv;
}

auto Canvas::pane_base( Uuid const& subdiv )
    -> Result< float >
{
    auto rv = KMAP_MAKE_RESULT( float );

    KMAP_ENSURE( rv, is_pane( subdiv ), error_code::network::invalid_node );

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

    KMAP_ENSURE( rv, is_pane( pane ), error_code::network::invalid_node );

    auto const hidden_node = KMAP_TRY( kmap_.fetch_child( pane, "hidden" ) );
    auto const hidden_body = KMAP_TRY( kmap_.fetch_body( hidden_node ) );

    rv = KMAP_TRY( from_string< bool >( hidden_body ) );

    return rv;
}

auto Canvas::pane_subdivision( Uuid const& subdiv )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( rv, is_pane( subdiv ), error_code::network::invalid_node );

    rv = kmap_.fetch_child( subdiv, "subdivision" );
    
    return rv;
}

auto Canvas::pane_path( Uuid const& subdiv )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const canvas_root = KMAP_TRY( fetch_canvas_root( kmap_ ) );
    auto const ids = kmap_.absolute_path_uuid( KMAP_TRY( make< Lineal >( kmap_, canvas_root, subdiv ) ) );

    KMAP_ENSURE( rv, is_pane( subdiv ), error_code::network::invalid_node );

    auto const headings = ids
       | views::stride( 2 ) // Drop "subdivision" from path.
       | views::transform( [ & ]( auto const& e ){ return kmap_.fetch_heading( e ).value(); } )
        | to< StringVec >();
    rv = headings
       | views::join( '.' )
       | to< std::string >();

    return rv;
}

auto Canvas::dimensions( Uuid const& target )
    -> Result< Dimensions >
{
    auto rv = KMAP_MAKE_RESULT( Dimensions );
    auto const canvas_root = KMAP_TRY( fetch_canvas_root( kmap_ ) ); 

    KMAP_ENSURE( rv, kmap_.is_lineal( canvas_root, target ), error_code::network::invalid_lineage );

    if( target == canvas_root )
    {
        rv = Dimensions{ .top=0
                       , .bottom=height()
                       , .left=0
                       , .right=width() };
    }
    else
    {
        KMAP_ENSURE( rv, is_pane( target ), error_code::network::invalid_node );

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
        auto const siblings = kmap_.fetch_siblings_inclusive_ordered( target );
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

auto Canvas::focus( Uuid const& pane )
    -> Result< void >
{
    using emscripten::val;
    
    auto rv = KMAP_MAKE_RESULT( void );

KMAP_LOG_LINE();
    KMAP_TRY( js::eval_void( io::format( "document.getElementById( '{}' ).focus();"
                                       , pane ) ) );
KMAP_LOG_LINE();

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