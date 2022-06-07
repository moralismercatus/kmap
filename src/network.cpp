/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "network.hpp"

#include "contract.hpp"
#include "db.hpp"
#include "error/network.hpp"
#include "event/event_clerk.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "option/option.hpp"
#include "utility.hpp"

#include <emscripten.h>
#include <emscripten/val.h>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/action/sort.hpp>

using namespace ranges;
using boost::uuids::to_string;
using emscripten::val;

namespace kmap
{

auto operator<<( std::ostream& os
               , Position2D const& lhs )
    -> std::ostream&
{
    os << '{' << lhs.x << ',' << lhs.y << '}';

    return os;
}

Network::Network( Kmap& kmap
                , Uuid const& container )
   : kmap_{ kmap }
   , oclerk_{ kmap }
   , eclerk_{ kmap }
   , js_nw_{ std::make_shared< val >( val::global().call< val >( "new_network", container ) ) } // TODO: There's some way to invoke a ctor directly without a wrapper, but can't figure it out.
{
    if( js_nw_->isNull() )
    {
        KMAP_THROW_EXCEPTION_MSG( "failed to initialize Network" );
    }
}

Network::~Network()
{
    if( js_nw_ )
    {
        js_nw_->call< val >( "destroy_network" );
    }
}

auto Network::create_node( Uuid const& id
                         , Title const& label )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( id ) );
                BC_ASSERT( fetch_title( id ).value() == label );
            }
        })
    ;

    KMAP_ENSURE( !exists( id ), error_code::network::invalid_node );

    auto const& sid = to_string( id );

    js_nw_->call< val >( "create_node"
                       , sid
                       , label ); // Note: I'd like to have markdown_to_html( title ) here, but visjs doesn't support HTML labels.
    // TODO: Remake to something like this:
    // KMAP_TRY( js::call< val >( *js_nw_, "select_node", to_string( id ) ) );

    rv = outcome::success();

    return rv; 
}

auto Network::add_edge( Uuid const& from
                      , Uuid const& to )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // Ensure edge exists.
            if( rv )
            {
                BC_ASSERT( edge_exists( from, to ) );
            }
        })
    ;

    // Ensure that title does not conflict with an existing path.
    // TODO: unsure if this should be checked here or in body.
    KMAP_ENSURE( exists( from ), error_code::network::invalid_node );
    KMAP_ENSURE( exists( to ), error_code::network::invalid_node );
    KMAP_ENSURE( !edge_exists( from, to ), error_code::network::invalid_edge );
    KMAP_ENSURE( !is_child( from, fetch_title( to ).value() ), error_code::network::invalid_node );

    js_nw_->call< val >( "add_edge"
                       , to_string( make_edge_id( from, to ) )
                       , to_string( from )
                       , to_string( to ) );

    rv = outcome::success();

    return rv;
}

auto Network::create_nodes( std::vector< Uuid > const& ids
                          , std::vector< Title > const& titles )
    -> void // TODO: Should return bool to indicate succ/fail.
{
    KMAP_PROFILE_SCOPE();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( ids.size() == titles.size() ); // TODO: This should return false on failure.
        })
    ;

    // TODO: Reimplement.
    //       In switching to a class-based impl., where the ownership is held by kmap::Network instance, I disabled the following because
    //       I couldn't figure out how to pass the network instance to EM_ASM_ARGS. Possibly via embind's interface e.g., val::global( "Module.VectorString" ).new_( ... )
    assert( false && "TODO: reimpl. needed." );
#if 0
    using emscripten::val;

    auto const sids = ids
                    | views::transform( [ & ]( auto const& e ){ return to_string( e ); } )
                    | to< std::vector< std::string > >();

    EM_ASM_ARGS
    (
        {
            var v_ids = new Module.VectorString( $0 );
            var v_titles = new Module.VectorString( $1 );
            // Convert C++ vectors to JS arrays.
            var js_ids = [];
            var js_titles = [];

            for( var i = 0
               ; i < v_ids.size()
               ; i++ )
            {
                var x = v_ids.get( i );
                js_ids.push( x );
            }
            for( var i = 0
               ; i < v_titles.size()
               ; i++ )
            {
                var x = v_titles.get( i );
                js_titles.push( x );
            }

            create_nodes( js_ids
                        , js_titles );
        }
        ,   &sids
        ,   &titles
    );
#endif // 0
}

auto Network::create_edges( std::vector< std::pair< Uuid, Uuid > > const& edges )
    -> void // TODO: Should return bool to indicate succ/fail.
{
    KMAP_PROFILE_SCOPE();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    // TODO: Reimplement.
    //       In switching to a class-based impl., where the ownership is held by kmap::Network instance, I disabled the following because
    //       I couldn't figure out how to pass the network instance to EM_ASM_ARGS. Possibly via embind's interface e.g., val::global( "Module.VectorString" ).new_( ... )
    assert( false && "TODO: reimpl. needed." );
#if 0
    using emscripten::val;

    auto const edge_ids = edges
                        | views::transform( [ & ]( auto const& e ){ return to_string( make_edge_id( e.first, e.second ) ); } )
                        | to< std::vector< std::string > >();
    auto const froms = edges
                     | views::transform( [ & ]( auto const& e ){ return to_string( e.first ); } )
                     | to< std::vector< std::string > >();
    auto const tos = edges
                   | views::transform( [ & ]( auto const& e ){ return to_string( e.second ); } )
                   | to< std::vector< std::string > >();

    EM_ASM_ARGS
    (
        {
            var v_edge_ids = new Module.VectorString( $0 );
            var v_froms = new Module.VectorString( $1 );
            var v_tos = new Module.VectorString( $2 );
            // Convert C++ vectors to JS arrays.
            var js_edge_ids = [];
            var js_froms = [];
            var js_tos = [];

            for( var i = 0
               ; i < v_edge_ids.size()
               ; i++ )
            {
                var x = v_edge_ids.get( i );
                js_edge_ids.push( x );
            }
            for( var i = 0
               ; i < v_froms.size()
               ; i++ )
            {
                var x = v_froms.get( i );
                js_froms.push( x );
            }
            for( var i = 0
               ; i < v_tos.size()
               ; i++ )
            {
                var x = v_tos.get( i );
                js_tos.push( x );
            }

            create_edges( js_edge_ids
                        , js_froms
                        , js_tos );
        }
        , &edge_ids
        , &froms
        , &tos
    );
#endif // 0
}

auto Network::select_node( Uuid const& id )
    -> Result< Uuid >
{
    KMAP_PROFILE_SCOPE();

    auto rv = KMAP_MAKE_RESULT_EC( Uuid, error_code::network::no_prev_selection );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap_.exists( id ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( id == selected_node() );
                BC_ASSERT( exists( rv.value() ) );
            }
        })
    ;

    auto prev_sel = js::call< val >( *js_nw_, "selected_node_id" );
    auto const visible_nodes = kmap_.fetch_visible_nodes_from( id );
    auto const visible_node_set = UuidSet{ visible_nodes.begin(), visible_nodes.end() };
    auto create_edge = [ & ]( auto const& nid )
    {
        if( auto const pid = kmap_.fetch_parent( nid )
          ; pid
         && visible_node_set.count( pid.value() ) != 0
         && !edge_exists( pid.value(), nid ) )
        {
            KTRYE( add_edge( pid.value(), nid ) );
        }
    };
    auto const to_title = [ & ]( auto const& e )
    {
        auto const title = KTRYE( kmap_.fetch_title( e ) );
        auto const child_count = kmap_.fetch_children( e ).size();

        return fmt::format( "{} ({})"
                          , title
                          , child_count );
    };

    // This is an inefficient hack. It appears recent versions of visjs will render hierarchy based on node chronology
    // So, to get around this, all nodes are deleted for each movement. Then, recreated, in order.
    remove_nodes();

    // Must be created in order, to work correctly with visjs's hierarchy mechanism.
    for( auto const& cid : visible_nodes )
    {
        BC_ASSERT( kmap_.exists( cid ) );
        KMAP_TRY( create_node( cid, to_title( cid ) ) );
        create_edge( cid );
    }

    // TODO: Think this is only necessary when we're not clearing all nodes each selection.
    // Revert previous selected node to unselected color.
    // if( visible_node_set.contains( prev_selected ) )
    // {
    //     color_node_background( prev_selected, Color::white );
    // }

    for( auto const& e : visible_nodes )
    {
        color_node( e ); 

        KMAP_TRY( change_node_font( e, get_appropriate_node_font_face( e ), Color::black ) );
    }

    KMAP_TRY( js::call< val >( *js_nw_, "select_node", to_string( id ) ) );

    color_node_background( id, Color::black );
    KTRY( change_node_font( id, get_appropriate_node_font_face( id ), Color::white ) );
    center_viewport_node( id );

    if( auto r = kmap_.option_store().apply( "network.viewport_scale" )
      ; !r && r.error().ec != error_code::network::invalid_node ) // TODO: Bit of a hack: when select_node() called before option set up, or after it's destroyed.
    {
        KTRY( r );
    }

    focus();

    if( prev_sel )
    {
        auto const pid = prev_sel.value().as< std::string >();

        rv = uuid_from_string( pid );
    }

    return rv;
}

auto Network::selected_node() const
    -> Uuid
{
    using emscripten::val;

    auto const nid = js::call< val >( *js_nw_, "selected_node_id" );

    if( !nid )
    {
        KMAP_THROW_EXCEPTION_MSG( "network has no node selected" );
    }
    
    auto const& sid = nid.value().as< std::string >();
    
    return uuid_from_string( sid ).value();
}

auto Network::fetch_parent( Uuid const& id ) const
    -> Result< Uuid >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nid = KMAP_TRY( js::call< val >( *js_nw_, "parent_id", to_string( id ) ) );
    auto const& sid = nid.as< std::string >();
    
    rv = uuid_from_string( sid );
    
    return rv;
}

auto Network::children( Uuid const& parent ) const
    -> std::vector< Uuid >
{
    using emscripten::val;
    using emscripten::vecFromJSArray;

    val ns = js_nw_->call< val >( "children_ids"
                                , to_string( parent ) );

    auto tv = vecFromJSArray< std::string >( ns ) ;
    auto sf = [ & ]( auto const& lhs 
                   , auto const& rhs )
    {
        return position( uuid_from_string( lhs ).value() ).y < position( uuid_from_string( rhs ).value() ).y;
    };

    tv |= actions::sort( sf );

    return tv
         | views::transform( []( auto const& e ){ return uuid_from_string( e ).value(); } )
         | to_vector;
}

auto Network::color_node( Uuid const& id
                     , Color const& color )
   -> void
{
    color_node_border( id, color );
}

auto Network::color_node( Uuid const& id )
   -> void
{
    auto const card = view::make( id ) | view::ancestor | view::count( kmap_ );
    auto const color = color_level_map[ ( card ) % color_level_map.size() ];

    color_node( id, color );
}

auto Network::child_titles( Uuid const& parent ) const
    -> std::vector< Title >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
        })
    ;

    auto const cts = children( parent );

    return cts 
         | views::transform( [ & ]( auto const& e ){ return KMAP_TRYE( this->fetch_title( e ) ); } )
         | to_vector;
}

auto Network::color_node_border( Uuid const& id
                               , Color const& color )
   -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( js_nw_ );
            BC_ASSERT( exists( id ) );
        })
    ;

    using emscripten::val;

    js_nw_->call< val >( "color_node_border"
                       , to_string( id )
                       , to_string( color ) );
}

auto Network::color_node_background( Uuid const& id
                                   , Color const& color )
   -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( js_nw_ );
            BC_ASSERT( exists( id ) );
        })
    ;

    using emscripten::val;

    js_nw_->call< val >( "color_node_background"
                       , to_string( id )
                       , to_string( color ) );
}

auto Network::change_node_font( Uuid const& id
                              , std::string const& face
                              , Color const& color )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( js_nw_ );
        })
    ;

    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );

    js_nw_->call< val >( "change_node_font"
                       , to_string( id )
                       , face
                       , to_string( color ) );

    rv = outcome::success();

    return rv;
}

auto Network::siblings_inclusive( Uuid const& id ) const
    -> std::tuple< std::vector< Uuid >
                 , uint32_t >
{
    auto const parent = fetch_parent( id );

    if( !parent )
    {
        return {};
    }

    auto const ss = children( parent.value() );
    auto const it = find( ss
                        , id );

    assert( it != end( ss ) );

    return { ss
           , distance( begin( ss )
                     , it ) };
}

auto Network::siblings_exclusive( Uuid const& id ) const
    -> std::vector< Uuid >
{
    auto const [ ss, ignore ] = siblings_inclusive( id );

    return ss 
         | views::remove( id )
         | to_vector;
}

auto Network::center_viewport_node( Uuid const& id )
    -> void
{
    js_nw_->call< val >( "center_viewport_node"
                       , to_string( id ) );
}

auto Network::exists( Uuid const& id ) const
    -> bool
{
    KMAP_PROFILE_SCOPE();

    return js_nw_->call< bool >( "id_exists"
                               , to_string( id ) );
}

auto Network::exists( Title const& title ) const
    -> bool
{
    KMAP_PROFILE_SCOPE();

    return 0 != count_if( nodes()
                        , [ & ]( auto const& e ){ return KMAP_TRYE( this->fetch_title( e ) ) == title; } );
}

auto Network::edge_exists( Uuid const& from
                         , Uuid const& to ) const
    -> bool
{
    KMAP_PROFILE_SCOPE();

    return js_nw_->call< bool >( "edge_exists"
                               , to_string( make_edge_id( from, to ) ) );
}

auto Network::focus()
    -> void
{
    KMAP_PROFILE_SCOPE();

    js_nw_->call< val >( "focus_network" );
}

// TODO: This should be gotten from options.
auto Network::get_appropriate_node_font_face( Uuid const& id ) const
    -> std::string
{
    auto rv = std::string{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
    ;

    if( kmap_.is_top_alias( id ) )
    {
        rv = "ariel";
    }
    else
    {
        rv = "verdana";
    }

    return rv;
}

auto Network::viewport_scale() const
    -> float
{
    return js_nw_->call< float >( "viewport_scale" );
}

auto Network::install_default_options()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    
    {
        auto const script = 
R"%%%(
kmap.network().scale_viewport( option_value ).throw_on_error();
)%%%";
        KMAP_TRY( oclerk_.install_option( "network.viewport_scale"
                                        , "Sets network's viewport scale after resize"
                                        , "1.0"
                                        , script ) );
    } 
    { // TODO: Too generic to belong in Network, but placing here for convenience.
        // TODO: This is ok, it works, but it shouldn't apply to shift/alt/ctrl. They should be exceptions.
        //       How best to communicate these exceptions? For one, there is the regex solution: "object.keyboard.key.[^shift|alt|ctrl]" (or something similar).
        //       The trouble with this is that it doesn't represent valid path syntax. Also, not the biggest fan of regex syntax.
        //       _An_ option would be to use the HeadingPath class that uses variant< std::string, std::regex > as components of the path.
        //       Similarly, variant< std::string, node_view::Intermediary > could perform the function well, where Intermediary describes:
        //       `view::direct_desc( "object.keyboard" ) | view::child( view::none_of( "shift", "alt", "ctrl" ) )`;
        //       And reset_transitions() understands that the Intermediary needs to be placed at the RHS  e.g., `view::make( event_root ) | path`.
        //       Of course... I can't use view_node concepts from JS at this time. Bleh...
        auto const script = 
R"%%%(
const fn = function(){ kmap.event_store().reset_transitions( to_VectorString( [ 'subject.network', 'verb.depressed', 'object.keyboard.key' ] ) ).throw_on_error(); };
setTimeout( fn, option_value );
)%%%";
        KMAP_TRY( oclerk_.install_option( "keyboard.key.timeout"
                                        , "Tells the event system to reset transitions for keyboard keys after given time interval."
                                        , "1000"
                                        , script ) );
    }


    rv = outcome::success();

    return rv;
}

auto Network::install_default_events()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.install_subject( "kmap" ) );
    KTRY( eclerk_.install_subject( "network" ) );
    KTRY( eclerk_.install_subject( "window" ) );
    KTRY( eclerk_.install_verb( "depressed" ) );
    KTRY( eclerk_.install_verb( "raised" ) );
    KTRY( eclerk_.install_verb( "scaled" ) );
    KTRY( eclerk_.install_verb( "selected" ) );
    KTRY( eclerk_.install_object( "keyboard.key.arrowdown" ) );
    KTRY( eclerk_.install_object( "keyboard.key.arrowleft" ) );
    KTRY( eclerk_.install_object( "keyboard.key.arrowright" ) );
    KTRY( eclerk_.install_object( "keyboard.key.arrowup" ) );
    KTRY( eclerk_.install_object( "keyboard.key.c" ) );
    KTRY( eclerk_.install_object( "keyboard.key.ctrl" ) );
    KTRY( eclerk_.install_object( "keyboard.key.e" ) );
    KTRY( eclerk_.install_object( "keyboard.key.esc" ) );
    KTRY( eclerk_.install_object( "keyboard.key.g" ) );
    KTRY( eclerk_.install_object( "keyboard.key.h" ) );
    KTRY( eclerk_.install_object( "keyboard.key.i" ) );
    KTRY( eclerk_.install_object( "keyboard.key.j" ) );
    KTRY( eclerk_.install_object( "keyboard.key.k" ) );
    KTRY( eclerk_.install_object( "keyboard.key.l" ) );
    KTRY( eclerk_.install_object( "keyboard.key.o" ) );
    KTRY( eclerk_.install_object( "keyboard.key.shift" ) );
    KTRY( eclerk_.install_object( "keyboard.key.v" ) );
    KTRY( eclerk_.install_object( "viewport" ) );
    KTRY( eclerk_.install_object( "node" ) );

    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.select_node"
                                      , .requisites = { "subject.kmap", "verb.selected", "object.node" }
                                      , .description = "updates network with selected node"
                                      , .action = R"%%%(kmap.network().select_node( kmap.selected_node() );)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_left.h"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.h" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_left();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_left.arrowleft"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowleft" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_left();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_down.j"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.j" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_down();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_down.arrowdown"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowdown" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_down();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_up.k"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.k" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_up();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_up.arrowup"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowup" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_up();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_right.l"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.l" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_right();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.travel_right.arrowright"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowright" }
                                      , .description = "travel to parent node"
                                      , .action = R"%%%(kmap.travel_right();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Branch{ .heading = "network.travel_bottom"
                                        , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.shift" }
                                        , .transitions = { Leaf{ .heading = "g"
                                                               , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                               , .description = "travel to bottom sibling."
                                                               , .action = R"%%%(kmap.travel_bottom();)%%%" }
                                                         , Leaf{ .heading = "unshift"
                                                               , .requisites = { "subject.network", "verb.raised", "object.keyboard.key.shift" }
                                                               , .description = "resets transition state."
                                                               , .action = R"%%%(/*Do nothing; Allow reset transition.*/)%%%" } } } ) );
    KTRY( eclerk_.install_outlet( Branch{ .heading = "network.travel_top"
                                        , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                        , .transitions = { Leaf{ .heading = "g"
                                                               , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                               , .description = "travel to top sibling."
                                                               , .action = R"%%%(kmap.travel_top();)%%%" } } } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.open_editor"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.e" }
                                      , .description = "open editor"
                                      , .action = R"%%%(kmap.parse_cli( ':edit.body' );)%%%" } ) ); // TODO: Replace parse_cli(...) with KScript{ "edit.body" }?
    // TODO: "leave_editor" belongs in editor, not here. Once "open_editor" occurs, control is handed to editor, no longer in network's domain.
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.leave_editor.esc"
                                      , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.esc" }
                                      , .description = "leave editor mode."
                                      , .action = R"%%%(kmap.leave_editor();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Branch{ .heading = "network.leave_editor.ctrl"
                                        , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.ctrl" }
                                        , .transitions = { Leaf{ .heading = "c"
                                                               , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.c" }
                                                               , .description = "leave editor mode."
                                                               , .action = R"%%%(kmap.leave_editor();)%%%" } } } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.update_viewport_scale_on_network_resize"
                                      , .requisites = { "subject.network", "verb.scaled", "object.viewport" }
                                      , .description = "updates network viewport scale option value"
                                      , .action = R"%%%(kmap.option_store().update_value( 'network.viewport_scale', kmap.network().viewport_scale() ).throw_on_error();)%%%" } ) );
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.refresh_on_window_resize"
                                      , .requisites = { "subject.window", "verb.scaled" }
                                      , .description = "resizes network when window resized"
                                      , .action = R"%%%(kmap.option_store().apply( 'network.viewport_scale' ).throw_on_error(); kmap.network().center_viewport_node( kmap.root_node() );)%%%" } ) );
    {
        // TODO: Too general to belong in Network, but placing here temporarily until more appropriate home is found.
        auto const script =
R"%%%(
kmap.option_store().apply( "keyboard.key.timeout" ).throw_on_error();
)%%%";
        KTRY( eclerk_.install_outlet( Leaf{ .heading = "keyboard.key.timeout"
                                          , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key" } // TODO: This ought not be limited to "subject.network", but any subject.
                                          , .description = "resets keyboard transition states aften timeout expiration"
                                          , .action = script } ) );
    }

    rv = outcome::success();
    
    return rv;
}

auto Network::update_title( Uuid const& id
                          , Title const& title )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
            // TODO: ensure no roots with same title in the same path.
        })
        BC_POST([ & ]
        {
            BC_ASSERT( this->fetch_title( id ).value() == title );
        })
    ;

    js_nw_->call< val >( "update_title"
                       , to_string( id )
                       , title );
}

auto Network::fetch_child( Uuid const& parent
                         , Title const& title ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const cids = children( parent );

    for( auto const& e : cids )
    {
        if( auto const ct = KMAP_TRY( this->fetch_title( e ) )
          ; ct == title )
        {
            rv = e;

            break;
        }
    }

    return rv;
}

auto Network::fetch_title( Uuid const& id ) const
    -> Result< Title >
{
    auto rv = KMAP_MAKE_RESULT( Title );
    auto const tv = KMAP_TRY( js::call< val >( *js_nw_, "title_of", to_string( id ) ) );

    rv = tv.as< std::string >();

    return rv;
}

auto Network::nodes() const
    -> std::vector< Uuid >
{
    using emscripten::vecFromJSArray;

    val ns = js_nw_->call< val >( "node_ids" );

    BC_ASSERT( ns.as< bool >() );

    auto const tv = vecFromJSArray< std::string >( ns ) ;

    return tv
         | views::transform( []( auto const& e ){ return uuid_from_string( e ).value(); } )
         | to_vector;
}

auto Network::is_child( Uuid parent
                      , Uuid const& child ) const
    -> bool
{
    return 0 != count( children( parent )
                     , child );
}

auto Network::is_child( Uuid parent
                      , Title const& child ) const
    -> bool
{
    return 0 != count_if( child_titles( parent )
                        , [ &child ]( auto const& e ){ return e == child; } );
}

auto Network::scale_viewport( float const scale )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    js_nw_->call< val >( "scale_viewport", scale );

    rv = outcome::success();

    return rv;
}

auto Network::to_titles( UuidPath const& path ) const
    -> StringVec
{
    return path
         | views::transform( [ & ]( auto const& e ){ return KMAP_TRYE( this->fetch_title( e ) ); } )
         | to< StringVec >();
}

auto Network::fetch_position( Uuid const& id ) const
    -> Result< Position2D >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Position2D );
    auto const ns = KMAP_TRY( js::call< val >( *js_nw_, "node_position", to_string( id ) ) );

    rv = Position2D{ ns[ "x" ].as< int32_t >()
                   , ns[ "y" ].as< int32_t >() };
    
    return rv;
}

auto Network::position( Uuid const& id ) const
    -> Position2D
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( fetch_position( id ) );
        })
    ;

    return fetch_position( id ).value(); // TODO: Propagate Result<>?
}

auto Network::erase_node( Uuid const& id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !exists( id ) );
            }
        })
    ;

    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );

    js_nw_->call< val >( "remove_node", to_string( id ) );
    
    rv = outcome::success();

    return rv;
}

auto Network::remove_nodes()
    -> void
{
    KMAP_PROFILE_SCOPE();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // TODO: BC_ASSERT( !( *js_nw_ )( "remove_nodes" ).isUndefined() );
        })
        BC_POST([ & ]
        {
            // TODO: assert all nodes and edges are removed.
        })
    ;

    // TODO: verify success.
    js_nw_->call< val >( "remove_nodes" );
}

auto Network::remove_edge( Uuid const& from
                         , Uuid const& to )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( !edge_exists( from, to ) );
        })
    ;

    KMAP_ENSURE( edge_exists( to, from ), error_code::network::invalid_edge );
    KMAP_ENSURE( exists( from ), error_code::network::invalid_node  );
    KMAP_ENSURE( exists( to ), error_code::network::invalid_node  );

    js_nw_->call< val >( "remove_edge"
                       , to_string( make_edge_id( from, to ) ) );

    rv = outcome::success();

    return rv;
}

auto Network::remove_edges()
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // TODO: BC_ASSERT( !( *js_nw_ )( "remove_edges" ).isUndefined() );
        })
        BC_POST([ & ]
        {
            //BC_ASSERT( val::global().call< val >( "network.body.data.edges.length == 0" ) );
        })
    ;

    // TODO: check if successful.
    js_nw_->call< val >( "remove_edges" );
}

auto Network::underlying_js_network()
    -> std::shared_ptr< emscripten::val >
{
    return js_nw_;
}

} // namespace kmap
