/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/network/visnetwork.hpp"

#include "com/canvas/canvas.hpp"
#include "com/database/db.hpp"
#include "com/event/event_clerk.hpp"
#include "com/network/network.hpp"
#include "com/option/option.hpp"
#include "component.hpp"
#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "error/network.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "test/util.hpp"
#include "util/result.hpp"
#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <emscripten/val.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/intersperse.hpp>

using namespace ranges;
using boost::uuids::to_string;
using emscripten::val;

namespace kmap::com {

auto operator<<( std::ostream& os
               , Position2D const& lhs )
    -> std::ostream&
{
    os << '{' << lhs.x << ',' << lhs.y << '}';

    return os;
}

VisualNetwork::VisualNetwork( Kmap& km
                            , std::set< std::string > const& requisites
                            , std::string const& description )
    : Component{ km, requisites, description }
    , oclerk_{ km }
    , eclerk_{ km }
    , pclerk_{ km }
{
    KTRYE( register_standard_options() );
    KTRYE( register_standard_events() );
    KTRYE( register_panes() );
}

VisualNetwork::~VisualNetwork()
{
    if( js_nw_ )
    {
        js_nw_->call< val >( "destroy_network" );
    }
}

auto VisualNetwork::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( pclerk_.install_registered() );

    auto const canvas = KTRY( fetch_component< Canvas >() );
    auto const container = canvas->network_pane();
    
    js_nw_ = std::make_shared< val >( val::global().call< val >( "new_network", container ) ); // TODO: There's some way to invoke a ctor directly without a wrapper, but can't figure it out.

    if( js_nw_->isNull() )
    {
        KMAP_THROW_EXCEPTION_MSG( "failed to initialize Network" ); // TODO: return Result
    }

    KTRY( oclerk_.install_registered() );
    KTRY( eclerk_.install_registered() );
    KTRY( apply_static_options() );

    rv = outcome::success();

    return rv;
}

auto VisualNetwork::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const canvas = KTRY( fetch_component< Canvas >() );
    auto const container = canvas->network_pane();

    KTRY( pclerk_.check_registered() ); // Must check before re-creating visjs.

    js_nw_ = std::make_shared< val >( val::global().call< val >( "new_network", container ) ); // TODO: There's some way to invoke a ctor directly without a wrapper, but can't figure it out.

    if( js_nw_->isNull() )
    {
        KMAP_THROW_EXCEPTION_MSG( "failed to initialize Network" ); // TODO: return Result
    }

    KTRY( oclerk_.check_registered() );
    KTRY( eclerk_.check_registered() );
    KTRY( apply_static_options() );

    rv = outcome::success();

    return rv;
}

auto VisualNetwork::create_node( Uuid const& id
                               , Title const& label )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );
        KM_RESULT_PUSH_STR( "title", label );

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

auto VisualNetwork::add_edge( Uuid const& from
                            , Uuid const& to )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "src", from );
        KM_RESULT_PUSH_NODE( "dst", to );

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

auto VisualNetwork::apply_static_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const ostore = KTRY( fetch_component< com::OptionStore >() );

    KTRY( ostore->apply( "canvas.network.background.color" ) );

    rv = outcome::success();

    return rv;
}

auto VisualNetwork::create_nodes( std::vector< Uuid > const& ids
                                , std::vector< Title > const& titles )
    -> void // TODO: Should return bool to indicate succ/fail.
{
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

auto VisualNetwork::create_edges( std::vector< std::pair< Uuid, Uuid > > const& edges )
    -> void // TODO: Should return bool to indicate succ/fail.
{
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

// TODO: Add unit tests for:
//   - node coloring
auto VisualNetwork::select_node( Uuid const& id )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );

    auto rv = KMAP_MAKE_RESULT_EC( Uuid, error_code::network::no_prev_selection );
    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( nw->exists( id ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( id == selected_node() );
            }
        })
    ;

    auto prev_sel = js::call< val >( *js_nw_, "selected_node_id" );
    auto const visible_nodes = nw->fetch_visible_nodes_from( id, 5, 10 );
    auto const visible_node_set = UuidSet{ visible_nodes.begin(), visible_nodes.end() };
    auto create_edge = [ & ]( auto const& nid )
    {
        if( auto const pid = nw->fetch_parent( nid )
          ; pid
         && visible_node_set.count( pid.value() ) != 0
         && !edge_exists( pid.value(), nid ) )
        {
            KTRYE( add_edge( pid.value(), nid ) );
        }
    };

    // This is an inefficient hack. It appears recent versions of visjs will render hierarchy based on node chronology
    // So, to get around this, all nodes are deleted for each movement. Then, recreated, in order.
    remove_nodes();

    // Must be created in order, to work correctly with visjs's hierarchy mechanism.
    for( auto const& cid : visible_nodes )
    {
        BC_ASSERT( nw->exists( cid ) );
        KTRY( create_node( cid, format_node_label( km, cid ) ) );
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

    auto const ostore = KTRY( fetch_component< com::OptionStore >() );

    if( auto r = ostore->apply( "network.viewport_scale" )
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

auto VisualNetwork::selected_node() const
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

auto VisualNetwork::fetch_parent( Uuid const& id ) const
    -> Result< Uuid >
{
    using emscripten::val;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nid = KMAP_TRY( js::call< val >( *js_nw_, "parent_id", to_string( id ) ) );
    auto const& sid = nid.as< std::string >();
    
    rv = uuid_from_string( sid );
    
    return rv;
}

auto VisualNetwork::children( Uuid const& parent ) const
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

auto VisualNetwork::color_node( Uuid const& id
                              , Color const& color )
   -> void
{
    color_node_border( id, color );
}

auto VisualNetwork::color_node( Uuid const& id )
   -> void
{
    auto const& km = kmap_inst();
    auto const card = view::make( id ) | view::ancestor | view::count( km );
    auto const color = color_level_map[ ( card ) % color_level_map.size() ];

    color_node( id, color );
}

auto VisualNetwork::child_titles( Uuid const& parent ) const
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

auto VisualNetwork::color_node_border( Uuid const& id
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

auto VisualNetwork::color_node_background( Uuid const& id
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

auto VisualNetwork::change_node_font( Uuid const& id
                                    , std::string const& face
                                    , Color const& color )
    -> Result< void >
{
    using emscripten::val;

    KM_RESULT_PROLOG();

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

auto VisualNetwork::siblings_inclusive( Uuid const& id ) const
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

auto VisualNetwork::siblings_exclusive( Uuid const& id ) const
    -> std::vector< Uuid >
{
    auto const [ ss, ignore ] = siblings_inclusive( id );

    return ss 
         | views::remove( id )
         | to_vector;
}

auto VisualNetwork::center_viewport_node( Uuid const& id )
    -> void
{
    js_nw_->call< val >( "center_viewport_node"
                       , to_string( id ) );
}

auto VisualNetwork::exists( Uuid const& id ) const
    -> bool
{
    return js_nw_->call< bool >( "id_exists"
                               , to_string( id ) );
}

auto VisualNetwork::exists( Title const& title ) const
    -> bool
{
    return 0 != count_if( nodes()
                        , [ & ]( auto const& e ){ return KMAP_TRYE( this->fetch_title( e ) ) == title; } );
}

auto VisualNetwork::edge_exists( Uuid const& from
                         , Uuid const& to ) const
    -> bool
{
    return js_nw_->call< bool >( "edge_exists"
                               , to_string( make_edge_id( from, to ) ) );
}

auto VisualNetwork::focus()
    -> void
{
    js_nw_->call< val >( "focus_network" );
}

// TODO: This should be gotten from options. Rather, settable by options, but stored in the appropriate state elsewhere.
auto VisualNetwork::get_appropriate_node_font_face( Uuid const& id ) const
    -> std::string
{
    auto rv = std::string{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
    ;

    auto const nw = KTRYE( fetch_component< com::Network >() );

    if( nw->alias_store().is_alias( id ) )
    {
        rv = "courier new"; // TODO: Draw from option_store
    }
    else
    {
        rv = "verdana"; // TODO: Draw from option_store
    }

    return rv;
}

auto VisualNetwork::viewport_scale() const
    -> float
{
    return js_nw_->call< float >( "viewport_scale" );
}

auto VisualNetwork::register_standard_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    
    {
        auto const script = 
        R"%%%(
            kmap.visnetwork().scale_viewport( option_value ).throw_on_error();
        )%%%";
        KMAP_TRY( oclerk_.register_option( Option{ .heading = "network.viewport_scale"
                                                 , .descr = "Sets network's viewport scale after resize"
                                                 , .value = "1.0"
                                                 , .action = script } ) );
    } 
    { // TODO: Too generic to belong in Network, but placing here for convenience.
        // TODO: This is ok, it works, but it shouldn't apply to shift/alt/ctrl. They should be exceptions.
        //       How best to communicate these exceptions? For one, there is the regex solution: "object.keyboard.key.[^shift|alt|ctrl]" (or something similar).
        //       The trouble with this is that it doesn't represent valid path syntax. Also, not the biggest fan of regex syntax.
        //       _An_ option would be to use the HeadingPath class that uses variant< std::string, std::regex > as components of the path.
        //       Similarly, variant< std::string, node_view::Intermediary > could perform the function well, where Intermediary describes:
        //       `view::direct_desc( "object.keyboard" ) | view::child( view::none_of( "shift", "alt", "control" ) )`;
        //       And reset_transitions() understands that the Intermediary needs to be placed at the RHS  e.g., `view::make( event_root ) | path`.
        //       Of course... I can't use view_node concepts from JS at this time. Bleh...
        auto const script = 
        R"%%%(
            const fn = function(){ kmap.event_store().reset_transitions( to_VectorString( [ 'subject.network', 'verb.depressed', 'object.keyboard.key' ] ) ).throw_on_error(); };
            setTimeout( fn, option_value );
        )%%%";
        KMAP_TRY( oclerk_.register_option( Option{ "keyboard.key.timeout"
                                                 , "Tells the event system to reset transitions for keyboard keys after given time interval."
                                                 , "1000"
                                                 , script } ) );
    }


    rv = outcome::success();

    return rv;
}

auto VisualNetwork::update_title( Uuid const& id
                                , Title const& title )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );
        KM_RESULT_PUSH_STR( "title", title );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( this->fetch_title( id ).value() == title );
            }
        })
    ;

    // TODO: ensure no roots with same title in the same path.
    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );

    // TODO: Somehow wrap into Result<> returning fn?
    js_nw_->call< val >( "update_title"
                       , to_string( id )
                       , title );
    
    rv = outcome::success();

    return rv;
}

auto VisualNetwork::fetch_child( Uuid const& parent
                               , Title const& title ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_STR( "title", title );

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

auto VisualNetwork::fetch_title( Uuid const& id ) const
    -> Result< Title >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );

    auto rv = KMAP_MAKE_RESULT( Title );
    auto const tv = KMAP_TRY( js::call< val >( *js_nw_, "title_of", to_string( id ) ) );

    rv = tv.as< std::string >();

    return rv;
}

auto VisualNetwork::nodes() const
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

auto VisualNetwork::is_child( Uuid parent
                      , Uuid const& child ) const
    -> bool
{
    return 0 != count( children( parent )
                     , child );
}

auto VisualNetwork::is_child( Uuid parent
                      , Title const& child ) const
    -> bool
{
    return 0 != count_if( child_titles( parent )
                        , [ &child ]( auto const& e ){ return e == child; } );
}

auto VisualNetwork::scale_viewport( float const scale )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "scale", std::to_string( scale ) );

    auto rv = KMAP_MAKE_RESULT( void );

    js_nw_->call< val >( "scale_viewport", scale );

    rv = outcome::success();

    return rv;
}

auto VisualNetwork::to_titles( UuidPath const& path ) const
    -> StringVec
{
    return path
         | views::transform( [ & ]( auto const& e ){ return KMAP_TRYE( this->fetch_title( e ) ); } )
         | to< StringVec >();
}

auto VisualNetwork::fetch_position( Uuid const& id ) const
    -> Result< Position2D >
{
    using emscripten::val;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );

    auto rv = KMAP_MAKE_RESULT( Position2D );
    auto const ns = KMAP_TRY( js::call< val >( *js_nw_, "node_position", to_string( id ) ) );

    rv = Position2D{ ns[ "x" ].as< int32_t >()
                   , ns[ "y" ].as< int32_t >() };
    
    return rv;
}

auto VisualNetwork::position( Uuid const& id ) const
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

auto VisualNetwork::erase_node( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );

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

auto VisualNetwork::register_panes()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( pclerk_.register_pane( Pane{ .id = network_uuid
                                     , .heading = "network"
                                     , .division = Division{ Orientation::horizontal, 0.000f, false, "div" } } ) );

    rv = outcome::success();

    return rv;
}

auto VisualNetwork::register_standard_events()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // Initial
    eclerk_.register_outlet( Leaf{ .heading = "network.select_node_on_init"
                                , .requisites = { "verb.initialized", "object.kmap" }
                                , .description = "updates network with selected node"
                                , .action = fmt::format( R"%%%(kmap.visnetwork().select_node( kmap.network().selected_node() );)%%%", gen_uuid() ) } );
    eclerk_.register_outlet( Leaf{ .heading = "network.select_node_on_load"
                                , .requisites = { "verb.loaded", "object.kmap" }
                                , .description = "updates network with selected node"
                                , .action = fmt::format( R"%%%(kmap.visnetwork().select_node( kmap.network().selected_node() );)%%%", gen_uuid() ) } );
    // Movement
    // TODO:
    // Well, trying to figure out here the best way to handle network changes reflection in vis.
    // Of course, the verbose way is to handle each network-altering event (move, erase, create, etc.) and select node on it.
    // Another would be to somehow treat all altering verbs to one outlet such that a re-selection (update) was done.
    // TODO: Do these really belong in visnetwork? I don't think visnetwork is, strictly speaking, a dependecy for these. Rather, it's the network that's the dependency.
    eclerk_.register_outlet( Leaf{ .heading = "network.select_node"
                                 , .requisites = { "subject.network", "verb.selected", "object.node" }
                                 , .description = "updates network with selected node"
                                 , .action = R"%%%(debounce( function(){{ kmap.visnetwork().select_node( kmap.selected_node() ); }}, 'debounce_timer_select_node', 10 )();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.node_moved"
                                 , .requisites = { "subject.network", "verb.moved", "object.node" }
                                 , .description = "updates network with selected node"
                                 , .action = R"%%%(kmap.visnetwork().select_node( kmap.network().selected_node() );)%%%" } );
    // Keyboard
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_left.h"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.h" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_left();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_left.arrowleft"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowleft" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_left();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_down.j"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.j" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_down();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_down.arrowdown"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowdown" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_down();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_up.k"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.k" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_up();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_up.arrowup"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowup" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_up();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_right.l"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.l" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_right();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.travel_right.arrowright"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowright" }
                                 , .description = "travel to parent node"
                                 , .action = R"%%%(kmap.travel_right();)%%%" } );
    eclerk_.register_outlet( Branch{ .heading = "network.key.shift"
                                   , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.shift" }
                                   , .transitions = { Leaf{ .heading = "travel_bottom"
                                                          , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                          , .description = "travel to bottom sibling."
                                                          , .action = R"%%%(kmap.travel_bottom();)%%%" }
                                                    , Leaf{ .heading = "unshift"
                                                          , .requisites = { "subject.network", "verb.raised", "object.keyboard.key.shift" }
                                                          , .description = "resets transition state."
                                                          , .action = R"%%%(/*Do nothing; Allow reset transition.*/)%%%" } } } );
    eclerk_.register_outlet( Branch{ .heading = "network.key.g"
                                   , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                   , .transitions = { Leaf{ .heading = "travel_top"
                                                          , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                          , .description = "travel to top sibling."
                                                          , .action = R"%%%(kmap.travel_top();)%%%" } } } );
    eclerk_.register_outlet( Leaf{ .heading = "network.open_editor"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.e" }
                                 , .description = "open editor"
                                 , .action = R"%%%(kmap.cli().parse_cli( ':edit.body' );)%%%" } ); // TODO: Replace parse_cli(...) with KScript{ "edit.body" }?
    // TODO: "leave_editor" belongs in editor/text_area, not here. Once "open_editor" occurs, control is handed to editor, no longer in network's domain.
    eclerk_.register_outlet( Leaf{ .heading = "network.leave_editor.esc"
                                 , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.esc" }
                                 , .description = "leave editor mode."
                                 , .action = R"%%%(kmap.on_leaving_editor();)%%%" } );
    eclerk_.register_outlet( Branch{ .heading = "network.leave_editor.control"
                                   , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.control" }
                                   , .transitions = { Leaf{ .heading = "c"
                                                          , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.c" }
                                                          , .description = "leave editor mode."
                                                          , .action = R"%%%(kmap.on_leaving_editor();)%%%" } } } );
    eclerk_.register_outlet( Leaf{ .heading = "network.update_viewport_scale_on_network_resize"
                                 , .requisites = { "subject.network", "verb.scaled", "object.viewport" }
                                 , .description = "updates network viewport scale option value"
                                 , .action = R"%%%(kmap.option_store().update_value( 'network.viewport_scale', kmap.visnetwork().viewport_scale() ).throw_on_error();)%%%" } );
    eclerk_.register_outlet( Leaf{ .heading = "network.refresh_on_window_resize"
                                 , .requisites = { "verb.scaled", "object.window" }
                                 , .description = "resizes network when window resized"
                                 , .action = R"%%%(kmap.option_store().apply( 'network.viewport_scale' ).throw_on_error(); kmap.visnetwork().center_viewport_node( kmap.root_node() );)%%%" } );
    {
        // TODO: Too general to belong in Network, but placing here temporarily until more appropriate home is found.
        auto const script =
        R"%%%(
            kmap.option_store().apply( "keyboard.key.timeout" ).throw_on_error();
        )%%%";
        eclerk_.register_outlet( Leaf{ .heading = "keyboard.key.timeout"
                                     , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key" } // TODO: This ought not be limited to "subject.network", but any subject.
                                     , .description = "resets keyboard transition states aften timeout expiration"
                                     , .action = script } );
    }
    {
        auto const script =
        R"%%%(
            kmap.canvas().toggle_pane( kmap.canvas().text_area_pane() );
            kmap.canvas().update_all_panes();
        )%%%";
        eclerk_.register_outlet( Leaf{ .heading = "workspace.toggle_preview"
                                     , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.p" }
                                     , .description = "Toggles visibility of the preview pane."
                                     , .action = script } );
    }

    rv = outcome::success();
    
    return rv;
}

auto VisualNetwork::remove_nodes()
    -> void
{
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

auto VisualNetwork::remove_edge( Uuid const& from
                               , Uuid const& to )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "src", from );
        KM_RESULT_PUSH_NODE( "to", to );

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

auto VisualNetwork::remove_edges()
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

auto VisualNetwork::underlying_js_network()
    -> std::shared_ptr< emscripten::val >
{
    return js_nw_;
}

auto format_node_label( Kmap const& km
                      , Uuid const& node )
    -> std::string // TODO: Should be Result< std::string >?
{
    auto rv = std::string{};
    auto const nw = KTRYE( km.fetch_component< com::Network >() );
    auto const db_title = KTRYE( nw->fetch_title( node ) );
    auto const child_count = nw->fetch_children( node ).size();
    auto const tags = view::make( node )
                    | view::tag
                    | view::to_node_set( km );
    auto const tag_hs = tags
                        | views::transform( [ & ]( auto const& t ){ return KTRYE( nw->fetch_heading( t ) ); } )
                        | ranges::to< std::vector >()
                        | actions::sort;
    auto const tag_line = tag_hs
                        | views::intersperse( "][" )
                        | views::join
                        | ranges::to< std::string >();

    if( !tags.empty() )
    {
        rv = fmt::format( "{} ({})\n<i>[{}]</i>"
                        , db_title
                        , child_count
                        , tag_line );
    }
    else
    {
        rv = fmt::format( "{} ({})"
                        , db_title
                        , child_count );
    }

    return rv;
}


namespace binding
{
    using namespace emscripten;

    struct VisualNetwork
    {
        Kmap& km;

        VisualNetwork( Kmap& kmap )
            : km{ kmap }
        {
        }

        auto center_viewport_node( Uuid const& node )
            -> void
        {
            KTRYE( km.fetch_component< kmap::com::VisualNetwork >() )->center_viewport_node( node );
        }

        auto focus()
            -> void
        {
            KTRYE( km.fetch_component< kmap::com::VisualNetwork >() )->focus();
        }

        auto format_node_label( Uuid const& node )
            -> std::string
        {
            return com::format_node_label( km, node );
        }

        auto select_node( Uuid const& node )
            -> kmap::binding::Result< Uuid >
        {
            return KTRYE( km.fetch_component< kmap::com::VisualNetwork >() )->select_node( node );
        }

        auto scale_viewport( float const scale )
            -> kmap::binding::Result< void >
        {
            return KTRYE( km.fetch_component< kmap::com::VisualNetwork >() )->scale_viewport( scale );
        }

        auto viewport_scale()
            -> float
        {
            return KTRYE( km.fetch_component< kmap::com::VisualNetwork >() )->viewport_scale();
        }

        auto underlying_js_network()
            -> emscripten::val
        {
            return *( KTRYE( km.fetch_component< kmap::com::VisualNetwork >() )->underlying_js_network() );
        }
    };

    auto visnetwork()
        -> binding::VisualNetwork
    {
        return binding::VisualNetwork{ kmap::Singleton::instance() };
    }

    EMSCRIPTEN_BINDINGS( kmap_visnetwork )
    {
        function( "visnetwork", &kmap::com::binding::visnetwork );
        class_< kmap::com::binding::VisualNetwork >( "VisualNetwork" )
            .function( "center_viewport_node", &kmap::com::binding::VisualNetwork::center_viewport_node )
            .function( "focus", &kmap::com::binding::VisualNetwork::focus )
            .function( "format_node_label", &kmap::com::binding::VisualNetwork::format_node_label )
            .function( "scale_viewport", &kmap::com::binding::VisualNetwork::scale_viewport )
            .function( "select_node", &kmap::com::binding::VisualNetwork::select_node )
            .function( "underlying_js_network", &kmap::com::binding::VisualNetwork::underlying_js_network )
            .function( "viewport_scale", &kmap::com::binding::VisualNetwork::viewport_scale )
            ;
    }
} // namespace binding

namespace {
namespace network_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::VisualNetwork
,   std::set({ "canvas.workspace"s, "event_store"s, "option_store"s, "command_store"s })
,   "main display for nodes"
);

} // namespace network_def 
}

SCENARIO( "visnetwork event benchmarks", "[visnetwork][event][benchmark]" )
{
    auto const all_components = REQUIRE_TRY( fetch_listed_components() );

    KMAP_COMPONENT_FIXTURE_SCOPED( all_components );

    auto& km = kmap::Singleton::instance();
    auto const estore = REQUIRE_TRY( km.fetch_component< com::EventStore >() );

    GIVEN( "all components init'd" )
    {
        BENCHMARK( "fire_event: arrowright" )
        {
            return estore->fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.arrowright" } );
        };
    }
}

} // namespace kmap
