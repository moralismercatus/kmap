/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "network.hpp"

#include "contract.hpp"
#include "db.hpp"
#include "error/network.hpp"
#include "event/event.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
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

    uninstall_events();
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
            BC_ASSERT( exists( id ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( id == selected_node() );
            BC_ASSERT( !rv || exists( rv.value() ) );
        })
    ;

    if( auto const prev = js::call< val >( *js_nw_, "selected_node_id" )
      ; prev )
    {
        auto const pid = prev.value().as< std::string >();

        rv = uuid_from_string( pid ).value();
    }

    KMAP_TRY( js::call< val >( *js_nw_, "select_node", to_string( id ) ) );

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

auto Network::install_keydown_handler()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    
    KMAP_TRY( js::call< val >( *js_nw_, "install_keydown_handler" ) );

    return rv;
}

auto Network::install_events()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap_.event_store();

    KTRY( estore.install_subject( "network" ) );
    KTRY( estore.install_verb( "depressed" ) );
    KTRY( estore.install_object( "keyboard.key.arrowdown" ) );
    KTRY( estore.install_object( "keyboard.key.arrowleft" ) );
    KTRY( estore.install_object( "keyboard.key.arrowright" ) );
    KTRY( estore.install_object( "keyboard.key.arrowup" ) );
    KTRY( estore.install_object( "keyboard.key.c" ) );
    KTRY( estore.install_object( "keyboard.key.ctrl" ) );
    KTRY( estore.install_object( "keyboard.key.e" ) );
    KTRY( estore.install_object( "keyboard.key.g" ) );
    KTRY( estore.install_object( "keyboard.key.h" ) );
    KTRY( estore.install_object( "keyboard.key.i" ) );
    KTRY( estore.install_object( "keyboard.key.j" ) );
    KTRY( estore.install_object( "keyboard.key.k" ) );
    KTRY( estore.install_object( "keyboard.key.l" ) );
    KTRY( estore.install_object( "keyboard.key.o" ) );
    KTRY( estore.install_object( "keyboard.key.v" ) );

    KTRY( estore.install_outlet( "network.travel_left"
                               , Transition::Branch{ Transition{ .heading = "h"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.h" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_left();)%%%" } }
                                                   , Transition{ .heading = "arrowleft"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowleft" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_left();)%%%" } } } ) );
    KTRY( estore.install_outlet( "network.travel_down"
                               , Transition::Branch{ Transition{ .heading = "j"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.j" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_down();)%%%" } }
                                                   , Transition{ .heading = "arrowdown"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowdown" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_down();)%%%" } } } ) );
    KTRY( estore.install_outlet( "network.travel_up"
                               , Transition::Branch{ Transition{ .heading = "k"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.k" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_up();)%%%" } }
                                                   , Transition{ .heading = "arrowup"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowup" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_up();)%%%" } } } ) );
    KTRY( estore.install_outlet( "network.travel_right"
                               , Transition::Branch{ Transition{ .heading = "l"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.l" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_right();)%%%" } }
                                                   , Transition{ .heading = "arrowright"
                                                               , .type = Transition::Leaf{ .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.arrowright" }
                                                                                         , .description = "travel to parent node"
                                                                                         , .action = R"%%%(kmap.travel_right();)%%%" } } } ) );

    rv = outcome::success();
    
    return rv;
}

auto Network::uninstall_events()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap_.event_store();

    KTRY( estore.uninstall_outlet( "network.travel_left" ) );
    KTRY( estore.uninstall_outlet( "network.travel_down" ) );
    KTRY( estore.uninstall_outlet( "network.travel_up" ) );
    KTRY( estore.uninstall_outlet( "network.travel_right" ) );

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

auto Network::remove( Uuid const& id )
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
