/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/canvas.hpp>

#include <com/canvas/common.hpp>
#include <com/database/root_node.hpp>
#include <com/event/event.hpp>
#include <com/filesystem/filesystem.hpp>
#include <com/network/network.hpp>
#include <com/option/option.hpp>
#include <component.hpp>
#include <contract.hpp>
#include <error/master.hpp>
#include <error/network.hpp>
#include <error/node_manip.hpp>
#include <filesystem.hpp>
#include <path.hpp>
#include <path/act/abs_path.hpp>
#include <path/act/order.hpp>
#include <path/act/update_body.hpp>
#include <path/node_view2.hpp>
#include <test/util.hpp>
#include <util/json.hpp>
#include <util/macro.hpp>
#include <util/result.hpp>
#include <util/window.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <boost/filesystem.hpp>
#include <boost/json/parse.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/contains.hpp>
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

#include <fstream>
#include <string>
#include <regex>

namespace bjn = boost::json;
namespace rvs = ranges::views;

// TODO: drop using namespace in favor of rvs.
using namespace ranges;

namespace kmap::com {

Canvas::Canvas( Kmap& km
              , std::set< std::string > const& requisites
              , std::string const& description )
    : Component{ km, requisites, description }
    , eclerk_{ km }
{
    KM_RESULT_PROLOG();

    KTRYE( register_standard_events() );
}

Canvas::~Canvas()
{
    try
    {
        KM_RESULT_PROLOG();
        
        auto const& km = kmap_inst();
        auto const nw = KTRYE( fetch_component< com::Network >() );

        // TODO: This *should* implicitly erase all HTML elements recursively, but should I do it explicitly?
        //       Any benefit/need to do it explicitly?
        #if !KMAP_NATIVE
        KTRYW( js::erase_child_element( to_string( util_canvas_uuid ) ) );
        #endif // !KMAP_NATIVE

        if( auto const croot = view2::canvas::canvas_root
                             | act2::fetch_node( km )
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
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( ensure_root_initialized() );
    KTRY( install_events() );

    KTRY( eclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto Canvas::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( ensure_root_initialized() );
    KTRY( update_all_panes() ); // TODO: This shouldn't be here, right? It should be called on load_done event.
    // KTRY( update_overlays() );

    KTRY( install_events() );

    KTRY( eclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}

auto Canvas::install_events()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( fetch_component< com::EventStore >() );

    KTRY( estore->install_subject( "window" ) );
    KTRY( estore->install_verb( "scaled" ) );

    // window.onresize
    {
        window_events_.emplace_back( js::ScopedCode{ "window.onresize = function(){ console.log( 'firing window resize event' ); kmap.event_store().fire_event( to_VectorString( [ 'verb.scaled', 'object.window' ] ) ); };"
                                                   , "window.onresize = null;" } );
    }

    rv = outcome::success();

    return rv;
}

auto fetch_layout( FsPath const& path )
    -> Result< bjn::object >
{
    namespace fs = boost::filesystem;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path.string() );

    auto rv = result::make_result< bjn::object >();

    if( fs::exists( path ) )
    {
        if( auto ifs = std::ifstream{ path.string() }
          ; ifs.good() )
        {
            auto ec = bjn::error_code{};
            auto const layout = bjn::parse( ifs, ec );

            if( !ec )
            {
                if( layout.is_object() )
                {
                    rv = layout.as_object();
                }
            }
        }
        else
        {
            // TODO: report failed to open file...
        }
    }

    return rv;
}

auto Canvas::apply_layout( std::string const& layout )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "layout", layout );

    auto rv = result::make_result< void >();
    auto ss = [ & ]
    {
        auto tss = std::stringstream{};
        tss << layout;
        return tss;
    }();

    auto ec = bjn::error_code{};
    auto const parsed = bjn::parse( ss, ec );

    KMAP_ENSURE( !ec, error_code::common::uncategorized );
    KMAP_ENSURE( parsed.is_object(), error_code::common::uncategorized );

    KTRY( apply_layout( parsed.as_object() ) );

    rv = outcome::success();

    return rv;
}


auto Canvas::apply_layout( bjn::object const& layout )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const lpane = KTRY( com::fetch_pane( layout ) );
    auto const croot_id = util_canvas_uuid;

    KMAP_ENSURE( lpane.id == croot_id, error_code::common::uncategorized );

    for( auto const lsubdivs = com::fetch_subdivisions( layout )
       ; auto const& lsubdiv : lsubdivs )
    {
        KTRY( apply_layout( lsubdiv, croot_id ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::apply_layout( bjn::object const& layout
                         , Uuid const& parent_pane ) 
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const lpane = KTRY( com::fetch_pane( layout ) );
    auto const update = [ & ]( std::string const& child, std::string const& body ) -> Result< void >
    {
        auto const cn = KTRY( anchor::node( lpane.id ) | view2::child( child ) | act2::fetch_or_create_node( km ) );

        KTRY( anchor::node( cn ) | act2::update_body( km, body ) );

        return outcome::success();
    };

    KMAP_ENSURE( is_pane( parent_pane ), error_code::common::uncategorized );

    KTRY( nw->update_heading( lpane.id, lpane.heading ) );
    KTRY( nw->update_title( lpane.id, format_title( lpane.heading ) ) );
    KTRY( update( "orientation", to_string( lpane.division.orientation ) ) );
    KTRY( update( "base", std::to_string( lpane.division.base ) ) );
    KTRY( update( "hidden", fmt::format( "{}", lpane.division.hidden ) ) );
    KTRY( update( "type", lpane.division.elem_type ) );

    if( !( anchor::node( parent_pane )
         | view2::child( "subdivision" )
         | view2::child( lpane.id )
         | act2::exists( km ) ) )
    {
        auto const psub = KTRY( anchor::node( parent_pane )
                              | view2::child( "subdivision" )
                              | act2::fetch_node( km ) );
                        
        KTRY( nw->move_node( lpane.id, psub ) );
    }

    for( auto const lsubdivs = com::fetch_subdivisions( layout )
       ; auto const& lsubdiv : lsubdivs )
    {
        KTRY( apply_layout( lsubdiv, lpane.id ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::install_pane( Pane const& pane )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", pane.id );
        KM_RESULT_PUSH( "heading", pane.heading );

    auto rv = result::make_result< Uuid >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_pane( pane.id ) );
            }
        })
    ;

    auto& km = kmap_inst();
    auto const canvas_root = KTRY( view2::canvas::canvas_root
                                 | act2::fetch_or_create_node( km ) );

    rv = KTRY( subdivide( canvas_root, pane.id, pane.heading, pane.division ) );

    return rv;
}

auto Canvas::install_overlay( Overlay const& overlay )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", overlay.id );
        KM_RESULT_PUSH( "heading", overlay.heading );

    auto rv = result::make_result< Uuid >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_overlay( overlay.id ) );
            }
        })
    ;

    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const overlay_root = KTRY( view2::canvas::overlay_root
                                  | act2::fetch_or_create_node( km ) );
    auto const overlayn = KTRY( nw->create_child( overlay_root, overlay.id, overlay.heading ) );
    auto const canvas_root = KTRY( view2::canvas::canvas_root
                                 | act2::fetch_node( km ) );
    auto const typen = KTRY( anchor::node( overlayn )
                           | view2::child( "type" )
                           | act2::create_node( km ) );

    KTRY( nw->update_body( typen, overlay.elem_type ) );

    KTRY( create_html_child_element( overlay.elem_type, canvas_root, overlayn ) );

    rv = overlayn;

    return rv;
}

auto Canvas::complete_path( std::string const& path )
    -> StringVec
{
    auto rv = StringVec{};
    auto const& km = kmap_inst();
    if( auto const canvas_root = view2::canvas::canvas_root
                               | act2::fetch_node( km )
      ; canvas_root )
    {
        auto const inflated = std::regex_replace( path, std::regex{ "\\." }, ".subdivision." );
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = false;
    auto const& km = kmap_inst();
    auto const nw = KTRYE( fetch_component< com::Network >() );

    if( auto const overlay_root = view2::canvas::overlay_root
                                | act2::fetch_node( km )
      ; overlay_root )
    {
        rv = ( anchor::node( node ) | view2::child( "type" ) | act2::exists( km ) )
          && is_ancestor( *nw, overlay_root.value(), node );
    }

    return rv;
}

auto Canvas::is_pane( Uuid const& node ) const
    -> bool
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = false;
    auto const& km = kmap_inst();
    auto const nw = KTRYE( fetch_component< com::Network >() );

    if( auto const canvas_root = view2::canvas::canvas_root
                               | act2::fetch_node( km )
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
        }
    }

    return rv;
}

auto Canvas::set_breadcrumb( UuidVec const& bc )
    -> Result< void >
{
    KM_RESULT_PROLOG();

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

        #if !KMAP_NATIVE
        KTRY( js::eval_void( fmt::format( "const elem = document.getElementById( '{}' );"
                                          "elem.innerText = '{}';"
                                          "elem.onclick = function(){{ kmap.select_node( '{}' ).throw_on_error(); }};"
                                          "elem.style.color = 'black';"
                                        , to_string( subdiv )
                                        , title
                                        , to_string( target ) ) ) );
        #endif // !KMAP_NATIVE

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent_pane", parent_pane );
        KM_RESULT_PUSH_NODE( "pane", pane_id );
        KM_RESULT_PUSH_STR( "heading", heading );

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

[[deprecated( "prefer predictable IDs for panes" )]]
auto Canvas::subdivide( Uuid const& parent_pane
                      , Heading const& heading
                      , Division const& subdiv )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent_pane", parent_pane );
        KM_RESULT_PUSH_STR( "heading", heading );
        
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( is_pane( parent_pane ), error_code::canvas::invalid_subdiv );
    KMAP_ENSURE( is_valid_heading( heading ), error_code::network::invalid_heading );

    auto& km = kmap_inst();
    auto const parent_pane_subdivn = KTRY( anchor::node( parent_pane )
                                         | view2::child( "subdivision" )
                                         | act2::fetch_node( km ) );

    rv = create_subdivision( parent_pane_subdivn
                           , heading
                           , subdiv );

    return rv;
}

auto Canvas::update_overlays()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

#if !KMAP_NATIVE
    auto const& km = kmap_inst();
    auto const canvas_root = KTRY( view2::canvas::canvas_root
                                 | act2::fetch_node( km ) );

    for( auto const overlays = view2::canvas::overlay_root
                             | view2::child
                             | act2::to_node_set( km )
       ; auto const& overlay : overlays )
    {
        if( !js::exists( overlay ) )
        {
            KTRY( create_html_child_element( "div", canvas_root, overlay ) );
        }
    }
#endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

auto Canvas::update_all_panes()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const& km = kmap_inst();
    auto const canvas_root = KTRY( view2::canvas::canvas_root
                                 | act2::fetch_node( km ) );

    KTRY( update_pane( canvas_root ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::update_pane( Uuid const& pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

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
    KMAP_PROFILE_SCOPE();

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( fetch_component< com::Network >() );
    
    KMAP_ENSURE( is_pane( root ), error_code::network::invalid_node );

    // TODO: I think if I can check for the existence of the element, and create it if it doesn't exist, then I can load.
    //       The one piece I'm missing - I think - is the type of element ('text_area', 'div', etc.). If I have this info,
    //       No problem!
    // html element creation must happen before subdiv, as subdiv depends on parent element.
#if !KMAP_NATIVE
    auto const subdivn = KTRY( pane_subdivision( root ) );

    if( !js::exists( root ) )
    {
        KTRY( create_html_element( root ) );
    }
    if( auto const parent_pane = fetch_parent_pane( root )
      ; parent_pane )
    {
        if( auto const pe_id = js::fetch_parent_element_id( to_string( root ) )
          ; pe_id && pe_id.value() != to_string( parent_pane.value() ) )
        {
            KTRY( js::move_element( to_string( root ), to_string( parent_pane.value() ) ) );
        }
    }

    for( auto const& subdiv : nw->fetch_children( subdivn ) )
    {
        KTRY( update_pane_descending( subdiv ) );
    }

    // io::print( "updating pane: {}\n", kmap_.absolute_path_flat( root ) );

    {
        auto style = KTRY( js::fetch_style_member( to_string( root ) ) ); 

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
#endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

// TODO: What's diff between this and update_all_panes()?
// TODO: Ensure JS calls to this are debounced. No reason not to, I think.
auto Canvas::update_panes()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const& km = kmap_inst();
    auto const canvas_root = KTRY( view2::canvas::canvas_root
                                 | act2::fetch_node( km ) );

    KMAP_ENSURE( is_pane( canvas_root ), error_code::network::invalid_node );

    KTRY( update_pane_descending( canvas_root ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::rebase_internal( Uuid const& pane
                            , float const base )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );
        KM_RESULT_PUSH_STR( "base", std::to_string( base ) );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_pane( pane ), error_code::network::invalid_node );

    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto const basen = KTRY( anchor::node( pane )
                           | view2::child( "base" )
                           | act2::fetch_node( km ) );
    auto const baseb = KTRY( anchor::node( basen )
                           | act2::fetch_body( km ) );
    auto base_val_str = fmt::format( "{:.4f}", base );

    if( baseb != base_val_str )
    {
        KTRY( nw->update_body( basen, base_val_str ) );
    }

    // TODO: We don't want to reorder unnecessarily. Make sure the orders don't match, first, before updating and triggering a cache clearing.
    //       Ah, but there's a problem. Because we change base on enter/exit :edit, text_area.base gets updated, so the cache is cleared.
    //       Q: Should base be stored in the node info? For one, it's duplicated. It exists in both JS canvas node and kmap. Maybe kmap can do without?
    //          Thereby eliminating the possibilty of dissymmetry.
    // Ensure network displays them as ordered according to their position.
    {
        auto const parent = KTRY( nw->fetch_parent( pane ) );
        auto const children = anchor::node( parent )
                            | view2::child
                            | view2::order
                            | act2::to_node_vec( km )
                            | actions::sort( [ & ]( auto const& lhs, auto const& rhs ){ return KTRYE( pane_base( lhs ) ) < KTRYE( pane_base( rhs ) ); } );
        KTRY( nw->reorder_children( parent, children ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::rebase( Uuid const& pane
                   , float const base )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );
        KM_RESULT_PUSH_STR( "base", std::to_string( base ) );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( rebase_internal( pane, base ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reorient_internal( Uuid const& pane
                              , Orientation const& orientation )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    KTRY( anchor::node( pane )
        | view2::child( "orientation" )
        | act2::update_body( km, to_string( orientation ) ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::orient( Uuid const& pane
                   , Orientation const& orientation )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( reorient_internal( pane, orientation ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::redraw()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( update_all_panes() );

    rv = outcome::success();

    return rv;
}

auto Canvas::register_standard_events()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    {
        auto const action = 
R"%%%(
console.log( 'update_all_panes()' );
kmap.canvas().update_all_panes();
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "window.update_canvas_on_window_resize"
                                     , .requisites = { "verb.scaled", "object.window" }
                                     , .description = "updates canvas on window resize"
                                     , .action = action } );
    }
    {
        auto const action = 
R"%%%(
console.log( 'update_all_panes()' );
const opt_root = ktry( kmap.fetch_node( '/meta.setting.option' ) );
const optn = ktry( kmap.fetch_descendant( opt_root, 'canvas.initial_layout' ) );
ktry( kmap.option_store().apply( optn ) );
ktry( kmap.canvas().update_all_panes() ); 
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "window.update_panes_after_components_initialized"
                                     , .requisites = { "subject.kmap", "verb.initialized" }
                                     , .description = "Ensures panes are up to date after all components are initialized"
                                     , .action = action } );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::reorient( Uuid const& pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const prev_orient = KTRY( pane_orientation( pane ) );
    auto const orientation = [ & ]
    {
        switch( prev_orient )
        {
            default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( hide_internal( pane, true ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::hide_internal( Uuid const& pane
                          , bool const hidden )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE_MSG( nw->exists( pane ), error_code::network::invalid_node, to_string_elaborated( km, pane ) );
    KMAP_ENSURE_MSG( is_pane( pane ), error_code::network::invalid_node, to_string_elaborated( km, pane ) );

    auto const hiddenn = KTRY( anchor::node( pane )
                             | view2::child( "hidden" )
                             | act2::fetch_node( km ) );
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );
        
    auto rv = KMAP_MAKE_RESULT( void );

    #if !KMAP_NATIVE
    KTRY( js::create_html_canvas( to_string( id ) ) );
    #endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

auto Canvas::create_html_element( Uuid const& pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const& km = kmap_inst();

    #if !KMAP_NATIVE
    KMAP_ENSURE( !js::exists( pane ), error_code::common::uncategorized );
    #endif // !KMAP_NATIVE

    auto const canvas_root = KTRY( view2::canvas::canvas_root
                                 | act2::fetch_node( km ) );

    if( pane == canvas_root )
    {
        KTRY( create_html_root_element( canvas_root ) );
    }
    else
    {
        auto const nw = KTRY( fetch_component< com::Network >() );
        auto const parent_pane = KTRY( fetch_parent_pane( pane ) );
        auto const typen = KTRY( anchor::node( pane ) | view2::child( "type" ) | act2::fetch_node( km ) );
        auto const elem_type = KTRY( nw->fetch_body( typen ) );

        KTRY( create_html_child_element( elem_type, parent_pane, pane ) );
    }

    rv = outcome::success();

    return rv;
}

auto Canvas::create_html_child_element( std::string const& elem_type
                                      , Uuid const& parent_id
                                      , Uuid const& child_id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parnet_id", parent_id );
        KM_RESULT_PUSH_NODE( "child_id", child_id );
        KM_RESULT_PUSH_STR( "elem_type", elem_type );

    auto rv = KMAP_MAKE_RESULT( void );

    #if !KMAP_NATIVE
    KTRY( js::create_child_element( to_string( parent_id ), to_string( child_id ), elem_type ) );
    KTRY( js::set_tab_index( to_string( child_id ), next_tabindex_++ ) );
    #endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

auto Canvas::initialize_overlays()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( create_overlay( completion_overlay(), "completion_overlay"/*, "div"*/ ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::reset_breadcrumb( Uuid const& supdiv )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "supdiv", supdiv );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "target", target );

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
    auto const vsub = anchor::node( target );

    KTRY( vsub | view2::child( "orientation" ) | act2::fetch_or_create_node( km ) );
    KTRY( vsub | view2::child( "base" ) | act2::fetch_or_create_node( km ) );
    KTRY( vsub | view2::child( "hidden" ) | act2::fetch_or_create_node( km ) );
    KTRY( vsub | view2::child( "type" ) | act2::fetch_or_create_node( km ) );
    KTRY( vsub | view2::child( "subdivision" ) | act2::fetch_or_create_node( km ) );

    KTRY( vsub | view2::child( "orientation" ) | act2::update_body( km, to_string( subdiv.orientation ) ) );
    KTRY( vsub | view2::child( "base" ) | act2::update_body( km, io::format( "{:.4f}", subdiv.base ) ) );
    KTRY( vsub | view2::child( "hidden" ) | act2::update_body( km, kmap::to_string( subdiv.hidden ) ) );
    KTRY( vsub | view2::child( "type" ) | act2::update_body( km, subdiv.elem_type ) );

    rv = outcome::success();

    return rv;
}

// TODO: So, need to have a load variety here. Or maybe create the child, then a separate routine that reads overlays, checks for their existence, and creates
//       html element if needed, for loading purposes. That seems a better option, more consistent with the panes approach.
auto Canvas::create_overlay( Uuid const& id
                           , std::string const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );
        KM_RESULT_PUSH( "heading", heading );

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
    auto const overlay_root = KTRY( view2::canvas::overlay_root
                                  | act2::fetch_or_create_node( km ) );
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_NODE( "child", child );
        KM_RESULT_PUSH_STR( "heading", heading );

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
    KTRY( create_html_element( subdivn ) );

    rv = subdivn;

    return rv;
}

auto Canvas::create_subdivision( Uuid const& parent
                               , std::string const& heading
                               , Division const& subdiv )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_STR( "heading", heading );

    return create_subdivision( parent 
                             , gen_uuid()
                             , heading
                             , subdiv );
}

auto Canvas::create_html_root_element( Uuid const& root_pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root_pane", root_pane );

    auto rv = KMAP_MAKE_RESULT( void );

    #if !KMAP_NATIVE
    BC_ASSERT( !js::element_exists( to_string( root_pane ) ) );

    // TODO: Shouldn't... this be a child of the 'canvas' element that we created?
    //       Unless... we don't actually use the 'canvas' type, but rather divs only.
    //       And... what is the 'canvas' element's purpose then? Anyway, 'div's will simplify things, as I'm familiar with those.
    KTRY( js::eval_void( io::format( "let canvas_div = document.createElement( 'div' );"
                                     "canvas_div.id = '{}';"
                                     "let body_tag = document.getElementsByTagName( 'body' )[ 0 ];"
                                     "body_tag.appendChild( canvas_div );" 
                                    , to_string( root_pane ) ) ) );
    #endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

auto Canvas::ensure_root_initialized()
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const win_root = KTRY( view2::canvas::window_root
                              | act2::fetch_or_create_node( km ) );

    if( auto const croot = view2::canvas::canvas_root
                         | act2::fetch_node( km )
      ; croot )
    {
        KMAP_LOG_LINE();
        rv = croot.value();
    }
    else
    {
        KMAP_LOG_LINE();
        auto const ncroot = KTRY( nw->create_child( win_root, util_canvas_uuid, "canvas" ) );

        KTRY( make_subdivision( ncroot, { Orientation::horizontal, 0.0f, false } ) );
        KTRY( create_html_root_element( util_canvas_uuid ) );

        rv = ncroot;
    }

    fmt::print( "finished: Canvas::ensure_root_initialized\n" );

    return rv;
}

auto Canvas::reveal( Uuid const& pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( hide_internal( pane, false ) );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "subdiv", subdiv );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "subdiv", subdiv );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "subdiv", subdiv );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    KMAP_ENSURE_MSG( is_pane( subdiv ), error_code::canvas::invalid_pane, KTRYE( nw->fetch_heading( subdiv ) ) );

    rv = nw->fetch_child( subdiv, "subdivision" );
    
    return rv;
}

auto Canvas::pane_path( Uuid const& subdiv )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "subdiv", subdiv );

    auto rv = KMAP_MAKE_RESULT( std::string );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const ids = view2::canvas::canvas_root
                   | view2::desc( subdiv )
                   | view2::left_lineal
                   | act2::to_node_set( km );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );
    #if !KMAP_NATIVE
    KMAP_ENSURE( js::element_exists( to_string( pane ) ), error_code::canvas::invalid_pane );
    #endif // !KMAP_NATIVE
    
    for( auto const& sp : KTRY( fetch_subdivisions( pane ) ) )
    {
        KTRY( delete_pane( sp ) );
    }

    #if !KMAP_NATIVE
    KTRY( js::eval_void( fmt::format( "document.getElementById( '{}' ).remove();", to_string( pane ) ) ) );
    #endif // !KMAP_NATIVE
    KTRY( nw->erase_node( pane ) );

    rv = outcome::success();

    return rv;
}

auto Canvas::delete_subdivisions( Uuid const& pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    for( auto const children = anchor::node( pane )
                             | view2::child( "subdivision" )
                             | view2::child
                             | act2::to_node_set( km )
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "target", target );

    auto rv = KMAP_MAKE_RESULT( Dimensions );
    auto& km = kmap_inst();
    auto const canvas_root = KTRY( view2::canvas::canvas_root
                                 | act2::fetch_node( km ) ); 
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
                              | views::remove_if( [ & ]( auto const& e ) { return KTRYE( pane_hidden( e ) ); } )
                              | views::transform( [ & ]( auto const& e ){ return std::pair{ KTRYE( pane_base( e ) ), e }; } )
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
                        return std::max( target_it->first, min_pane_size_multiplier );
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
        auto const siblings = anchor::node( target )
                            | view2::sibling_incl
                            | view2::order
                            | act2::to_node_vec( km );
        auto const percs = compute_percents( siblings );
        auto const pdims = KTRY( dimensions( parent_pane ) );

        switch( orientation )
        {
            default:
            {
                KMAP_THROW_EXCEPTION_MSG( "expected horizontal or vertical orientation" );
            }
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

    return rv;
}

SCENARIO( "Canvas::dimensions", "[canvas]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "canvas" );

    auto& km = kmap::Singleton::instance();
    auto const& canvas = REQUIRE_TRY( km.fetch_component< com::Canvas >() );

    GIVEN( "default root pane" )
    {
        REQUIRE_RFAIL( canvas->dimensions( gen_uuid() ) ); // non-existent pane fails

        THEN( "standard root dimensions" )
        {
        }

        // GIVEN( "root pane" )
        // {
        //     REQUIRE_TRY( canvas->ensure_root_initialized() );
        //     // REQUIRE_TRY( canvasinstall_pane( Pane{ workspace_pane(), "workspace", Division{ Orientation::vertical, 0.025f, false, "div" } } ) );
        // }
    }
}

auto Canvas::fetch_parent_pane( Uuid const& node )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", node );
    
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );

    auto rv = KMAP_MAKE_RESULT( UuidSet );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( anchor::node( pane ) | view2::desc( "subdivision" ) | act2::exists( km ) );
        })
    ;

    KMAP_ENSURE( is_pane( pane ), error_code::canvas::invalid_pane );

    rv = anchor::node( pane )
       | view2::child( "subdivision" )
       | view2::child
       | act2::to_node_set( km );

    return rv;
}

auto Canvas::focus( Uuid const& pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "pane", pane );
    
    auto rv = KMAP_MAKE_RESULT( void );

    #if !KMAP_NATIVE
    KTRY( js::eval_void( io::format( "document.getElementById( '{}' ).focus();", pane ) ) );
    #endif // !KMAP_NATIVE

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", path );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const expanded = expand_path( path );
    auto const desc = KTRY( view2::canvas::window_root
                          | view2::desc( io::format( "{}", expanded ) )
                          | act2::fetch_node( km ) );

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

auto Canvas::jump_stack_pane() const
    -> Uuid
{
    return jump_stack_uuid;
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

namespace {
namespace canvas_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Canvas
,   std::set({ "command.store"s, "event_store"s, "option_store"s, "network"s, "root_node"s })
,   "canvas related functionality"
);

} // namespace canvas_def 
}

} // namespace kmap::com
