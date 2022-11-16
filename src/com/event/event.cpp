/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "event.hpp"

#include "cmd/parser.hpp"
#include "com/network/network.hpp"
#include "common.hpp"
#include "error/network.hpp"
#include "error/parser.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "path/act/abs_path.hpp"
#include "path/act/front.hpp"
#include "path/act/order.hpp"
#include "path/act/push.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"
#include "util/script/script.hpp"

#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/split.hpp>

#include <vector>

namespace rvs = ranges::views;

namespace kmap::com {

auto EventStore::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto EventStore::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto EventStore::action( std::string const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );
    
    rv = KTRY( view::make( eroot )
             | view::child( "action" )
             | view::desc( heading )
             | view::single
             | view::create_node( km )
             | view::to_single );

    return rv;
}

auto EventStore::object( std::string const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );
    
    rv = KTRY( view::make( eroot )
             | view::child( "object" )
             | view::desc( heading )
             | view::single
             | view::create_node( km )
             | view::to_single );

    return rv;
}

auto EventStore::event_root()
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const mroot = km.root_node_id();

    rv = KTRY( view::make( mroot )
             | view::direct_desc( "meta.event" ) 
             | view::fetch_or_create_node( km ) );

    return rv;
}

auto EventStore::event_root() const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const mroot = km.root_node_id();

    rv = KTRY( view::make( mroot )
             | view::direct_desc( "meta.event" ) 
             | view::fetch_node( km ) );

    return rv;
}

auto EventStore::fetch_outlet_base( Uuid const& node )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_outlet( rv.value() ) );
            }
        })
    ;

    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );
    auto const ofront = KTRY( view::make( eroot )
                            | view::child( "outlet" )
                            | view::rlineage( node )
                            | view::child( "requisite" )
                            | view::child/*( TODO: aliases to any subject/verb/object )*/
                            | view::parent
                            | view::parent
                            | view::to_node_set( km )
                            | ranges::to< std::vector >()
                            | act::order( km )
                            | act::front );

    rv = ofront;

    return rv;
}

auto EventStore::fetch_outlet_tree( Uuid const& outlet )
    -> Result< UuidSet >
{
    KMAP_ENSURE( is_outlet( outlet ), error_code::network::invalid_node );

    auto rv = KMAP_MAKE_RESULT( UuidSet );
    auto& km = kmap_inst();
    auto const obase = KTRY( fetch_outlet_base( outlet ) );
    auto const otree = view::make( obase )
                     | view::desc
                     | view::child( "requisite" )
                     | view::parent
                     | view::to_node_set( km )
                     | act::push( obase );

    rv = otree;

    return rv;
}

auto EventStore::fetch_payload()
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );

    if( payload_ )
    {
        rv = payload_.value();
    }

    return rv;
}

auto EventStore::install_verb( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const eroot = KTRY( event_root() );
    auto& km = kmap_inst();

    rv = view::make( eroot )
       | view::child( "verb" )
       | view::direct_desc( heading ) 
       | view::fetch_or_create_node( km ); // TODO: For all of these install_* fns, I'm doing a fetch_or_create for convenience, but the name "install" implies that it doesn't already exist. Either rename (ensure_subject_installed?) or return error.

    return rv;
}

auto EventStore::install_object( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    rv = KMAP_TRY( view::make( eroot )
                 | view::child( "object" )
                 | view::direct_desc( heading )
                 | view::fetch_or_create_node( km ) );

    return rv;
}

auto EventStore::install_subject( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    rv = KMAP_TRY( view::make( eroot )
                 | view::child( "subject" )
                 | view::direct_desc( heading )
                 | view::fetch_or_create_node( km ) );

    return rv;
}

auto EventStore::install_component( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    rv = KMAP_TRY( view::make( eroot )
                 | view::child( "component" )
                 | view::direct_desc( heading )
                 | view::fetch_or_create_node( km ) );

    return rv;
}

auto EventStore::install_outlet( Leaf const& leaf )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );
    auto const oroot = KTRY( view::make( eroot )
                           | view::child( "outlet" )
                           | view::direct_desc( leaf.heading )
                           | view::create_node( km )
                           | view::to_single );

    KTRY( install_outlet_internal( oroot, leaf ) );
    KTRY( reset_transitions( oroot ) );

    rv = oroot;

    return rv;
}

SCENARIO( "install_outlet", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();

    GIVEN( "requisites" )
    {
        auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );

        REQUIRE_RES( estore->install_subject( "victor" ) );
        REQUIRE_RES( estore->install_verb( "charlied" ) );
        REQUIRE_RES( estore->install_object( "delta" ) );

        WHEN( "install leaf" )
        {
            auto const leaf = Leaf{ .heading = "network.open_editor"
                                  , .requisites = { "subject.victor", "verb.charlied", "object.delta" }
                                  , .description = "VCD"
                                  , .action = "kmap.create_child(kmap.rood_node(), '1');" };
            auto const oleaf = REQUIRE_TRY( estore->install_outlet( leaf ) );

            THEN( "leaf structure exists" )
            {
                REQUIRE_RES(( view::make( oleaf ) 
                            | view::child( view::exactly( "requisite", "description", "action" ) )
                            | view::exists( kmap ) ));
            }
        }
    }
}

auto EventStore::install_outlet( Branch const& branch )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );
    auto const oroot = KTRY( view::make( eroot )
                           | view::child( "outlet" )
                           | view::direct_desc( branch.heading )
                           | view::create_node( km )
                           | view::to_single );

    KTRY( install_outlet_internal( oroot, branch ) );
    KTRY( reset_transitions( oroot ) );

    rv = oroot;

    return rv;
}

#if 0
auto EventStore::install_outlet_transition( Uuid const& root
                                          , Transition const& transition )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const transdest = KMAP_TRY( view::make( root )
                                   | view::child( "transition" )
                                   | view::child( transition.heading )
                                   | view::create_node( kmap_ )
                                   | view::to_single );
    auto const vres = std::visit( [ & ]( auto const& arg ) -> Result< void >
                                  {
                                      using T = std::decay_t< decltype( arg ) >;

                                      auto rv = KMAP_MAKE_RESULT( void );

                                      if constexpr( std::is_same_v< T, Transition::Leaf > )
                                      {
                                          KMAP_TRY( install_outlet_leaf( transdest, arg ) );

                                          rv = outcome::success();
                                      }
                                      else if constexpr( std::is_same_v< T, Transition::Branch > )
                                      {
                                          for( auto const& new_trans : arg )
                                          {
                                            KMAP_TRY( install_outlet_transition( transdest, new_trans ) );
                                          }

                                          rv = outcome::success();
                                      }
                                      else
                                      {
                                          static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                      }

                                      return rv;
                                  }
                                , transition.type );

    if( !vres )
    {
        rv = KMAP_PROPAGATE_FAILURE( vres );
    }
    else
    {
        rv = transdest;
    }

    return rv;
}
#endif

auto EventStore::install_outlet_internal( Uuid const& root 
                                        , Leaf const& leaf )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const eroot = KTRY( event_root() );

    KMAP_ENSURE( js::lint( leaf.action ), error_code::js::lint_failed );

    auto const action_body = util::to_js_body_code( js::beautify( leaf.action ) );

    KMAP_ENSURE( cmd::parser::parse_body_code( action_body ), error_code::parser::parse_failed );

    for( auto const& e : leaf.requisites )
    {
        KMAP_TRY( view::make( root )
                | view::child( "requisite" ) 
                | view::alias( view::make( eroot ) | view::desc( e ) ) 
                | view::create_node( km ) );
    }

    auto const descn  = KMAP_TRY( view::make( root )
                                | view::child( "description" ) 
                                | view::single
                                | view::create_node( km )
                                | view::to_single );
    auto const actionn = KMAP_TRY( view::make( root )
                                 | view::child( "action" ) 
                                 | view::single
                                 | view::create_node( km )
                                 | view::to_single );

    KMAP_TRY( nw->update_body( descn, leaf.description ) );
    KMAP_TRY( nw->update_body( actionn, action_body ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::install_outlet_internal( Uuid const& root 
                                        , Branch const& branch )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KMAP_ENSURE( !branch.transitions.empty(), error_code::common::uncategorized );

    for( auto const& e : branch.requisites )
    {
        auto const req_alias = KTRY( view::make( eroot )
                                   | view::desc( e )
                                   | view::fetch_node( km ) );
        KMAP_TRY( view::make( root )
                | view::child( "requisite" ) 
                | view::alias( req_alias ) 
                | view::create_node( km ) );
    }

    for( auto const& transition : branch.transitions )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( Leaf const& leaf ) -> Result< void >
            {
                auto const lroot = KTRY( view::make( root )
                                       | view::child( leaf.heading )
                                       | view::create_node( km )
                                       | view::to_single );

                return install_outlet_internal( lroot, leaf );
            }
        ,   [ & ]( Branch const& branch ) -> Result< void >
            {
                auto const broot = KTRY( view::make( root )
                                       | view::child( branch.heading )
                                       | view::create_node( km )
                                       | view::to_single );

                return install_outlet_internal( broot, branch );
            }
        };
        KTRY( std::visit( dispatch , transition ) );
    }
        
    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_subject( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KTRY( view::make( eroot )
        | view::child( "subject" )
        | view::direct_desc( heading )
        | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_subject( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KTRY( view::make( eroot )
        | view::child( "subject" )
        | view::desc( node )
        | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_verb( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KTRY( view::make( eroot )
        | view::child( "verb" )
        | view::direct_desc( heading )
        | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_verb( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KTRY( view::make( eroot )
        | view::child( "verb" )
        | view::desc( node )
        | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_object( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KMAP_TRY( view::make( eroot )
            | view::child( "object" )
            | view::direct_desc( heading )
            | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_object( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KTRY( view::make( eroot )
        | view::child( "object" )
        | view::desc( node )
        | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_component( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KTRY( view::make( eroot )
        | view::child( "component" )
        | view::desc( node )
        | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_outlet( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const eroot = KTRY( event_root() );

    KMAP_TRY( view::make( eroot )
            | view::child( "outlet" )
            | view::direct_desc( heading )
            | view::erase_node( km ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_outlet( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    KTRY( nw->erase_node( node ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_outlet_transition( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( fetch_component< com::Network >() );

    KTRY( nw->erase_node( node ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::execute_body( Uuid const& node )
    -> Result< void >
{    
    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( nw->exists( node ), error_code::network::invalid_node );

    auto const body = KMAP_TRY( nw->fetch_body( node ) );
    auto const code = KMAP_TRY( cmd::parser::parse_body_code( body ) );
    auto const vres = boost::apply_visitor( [ & ]( auto const& e ) -> Result< void >
                                          {
                                              using T = std::decay_t< decltype( e ) >;

                                              auto rv = KMAP_MAKE_RESULT( void );

                                              if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                                              {
                                                  KMAP_THROW_EXCEPTION_MSG( "TODO: Impl. needed" );
                                                  // rv = execute_kscript( kmap, e.code );
                                              }
                                              else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                                              {
                                                  KMAP_TRY( js::eval_void( e.code ) );
                                              }
                                              else
                                              {
                                                  static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                              }

                                              rv = outcome::success();

                                              return rv;
                                          }
                                          , code );

    if( !vres )
    {
        return KMAP_PROPAGATE_FAILURE( vres );
    }

    rv = outcome::success();
    
    return rv;
}

// TODO: Convenience, but non-core, so maybe free function? IDK...
auto EventStore::fetch_matching_outlets( std::set< std::string > const& requisites )
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );
    auto const& km = kmap_inst();
    auto const nw = KTRYE( fetch_component< com::Network >() );
    auto const ver = view::make( KTRYE( event_root() ) );
    auto const sreqs = ver
                     | view::direct_desc( view::all_of( requisites ) ) // TODO: is view::all_of an all or nothing operation? Verify. Needs to be for this case.
                     | view::to_node_set( km );

    KMAP_ENSURE( sreqs.size() == requisites.size(), error_code::common::uncategorized );

    auto const reqs_match = [ & ]( auto const& outlet )
    {
        auto const oreqs = view::make( outlet )
                         | view::child( "requisite" )
                         | view::alias
                         | view::resolve
                         | view::to_node_set( km );

        return ranges::distance( rvs::set_intersection( sreqs, oreqs ) ) == sreqs.size();
    };
    auto const all_outlets = ver
                           | view::child( "outlet" )
                           | view::child // TODO: This only accounts for immediate child nodes. I actually want view::desc( is_outlet )
                           | view::to_node_set( km );

    rv = all_outlets
       | rvs::filter( reqs_match )
       | ranges::to< UuidSet >();

    return rv;
}

SCENARIO( "EventStore::fetch_matching_outlets", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );

    GIVEN( "one requisite" )
    {
        REQUIRE_RES( estore->install_subject( "victor" ) );

        GIVEN( "outlet for requisite" )
        {
            auto const ores = estore->install_outlet( Leaf{ .heading = "1"
                                                          , .requisites = { "subject.victor" }
                                                          , .description = "test"
                                                          , .action = "/*NOP*/" } );
            REQUIRE_RES( ores );

            THEN( "matching outlet found" )
            {
                auto const mos = REQUIRE_TRY( estore->fetch_matching_outlets( { "subject.victor" } ) );

                REQUIRE( *mos.begin() == ores.value() );
            }
        }
    }
}

// TODO: If the requested subject, verb, or object is missing, don't err, it just means one hasn't been installed yet, so there'll be nothing to match against.
// TODO: Shouldn't be limited to number of objects (or subjects or verbs? Those seem a little unintuitive...)
// TODO: Currently, I think only `object` cascades. All should cascade (i.e., if an ancestor fires, descendants should be triggered as well).
auto EventStore::fire_event( std::set< std::string > const& requisites )
    -> Result< void >
{
    // TODO: log( "event_store.fire_event", fmt::format( "requisites: {{{}}}", requisites ) ); // => ( "[log][kmap.event.fire_event] requisites: {{{}}}", requisites )
    // TODO: Each component comes with a logging facility i.e., the function log() that automatically prefixes output with component name?
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    if( auto const oroot = view::abs_root
                         | view::direct_desc( "meta.event.object" )
                         | view::fetch_node( km )
      ; oroot )
    {
        auto const eroot = KTRY( view::make( oroot.value() )
                               | view::parent
                               | view::fetch_node( km ) );
        auto const objects = requisites
                        | ranges::views::filter( []( auto const& e ){ return e.starts_with( "object" ); } )
                        | ranges::to< std::set >();
        auto const others = requisites
                        | ranges::views::filter( []( auto const& e ){ return !e.starts_with( "object" ); } )
                        | ranges::to< std::set >();

        KMAP_ENSURE_MSG( objects.size() <= 1, error_code::common::uncategorized, "currently limiting object to a single entry (no conceptual limitation, just ease of impl." );

        for( auto const& opath : objects )
        {
            if( auto rnode = view::make( eroot )
                        | view::desc( opath )
                        | view::fetch_node( km )
            ; rnode )
            {
                auto node = rnode.value();
                
                while( oroot.value() != node )
                {
                    auto combined = others;
                    auto const abspath = KTRY( view::make( oroot.value() )
                                            | view::desc( node )
                                            | act::abs_path_flat( km ) );

                    combined.emplace( abspath );

                    KTRY( fire_event_internal( combined ) );

                    node = KTRY( nw->fetch_parent( node ) );
                }
            }
        }
    }

    rv = outcome::success();

    return rv;
}

auto EventStore::fire_event( std::set< std::string > const& requisites
                           , std::string const& payload )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    
    payload_ = payload;
    {
        KTRY( fire_event( requisites ) );
    }
    payload_ = std::nullopt; // TODO: payload_ needs to be nulled even if fire_event returns error.

    rv = outcome::success();

    return rv;
}

auto EventStore::fire_event_internal( std::set< std::string > const& requisites )
    -> Result< void >
{
    // fmt::print( "fire_event_internal: {}\n", requisites | ranges::views::join( '|' ) | ranges::to< std::string >() );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const ver = view::make( KTRY( event_root() ) );

    auto const vreqs = ver | view::direct_desc( view::all_of( requisites ) );
    auto const vor = ver | view::child( "outlet" );
    auto const vcr = ver | view::child( "component" );
    auto const outlets = [ & ]
    {
        // So, this is what I'm trying to get at:
        // All matches for usual requisites.
        // Component requisites are special.
        // Rather than being matched to the fired set, the outlets matching the fired set are examined for component requisites.
        // These components are then checked (km.component_store().is_initialized( com_id )) for initialized status,
        // and only then are the outlets fired.
        // TODO: Shouldn't it be view:child( view::exactly( vreqs ) ) instead of view::child( view::all_of( vreqs ) )?
        auto const all_matches = vor
                               | view::desc( "requisite" )
                               | view::child( view::all_of( vreqs | view::to_heading_set( km ) ) ) // TODO: Actually needs view::alias( reqs ), rather than just the heading matches.
                               | view::parent
                               | view::parent
                               | view::to_node_set( km )
                               | act::order( km );
        auto com_matches = decltype( all_matches ){};

        // Ensure all req coms have been initialized.
        for( auto const& outlet : all_matches )
        {
            auto const vcom_req = view::make( outlet )
                                | view::child( "requisite" )
                                | view::alias( view::PredFn{ [ & ]( auto const& n ){ return vcr | view::desc( nw->resolve( n ) ) | view::exists( km ); } } );

            if( vcom_req | view::exists( km ) )
            {
                for( auto const& creq : vcom_req 
                                      | view::resolve 
                                      | view::to_node_set( km ) )
                {
                    auto const abs_com_path = KTRYE( view::make( creq )
                                                   | view::ancestor( vcr )
                                                   | act::abs_path_flat( km ) );
                    auto const com_name = abs_com_path
                                        | ranges::views::split( '.' )
                                        | ranges::views::drop( 1 ) // Drop 'component.'
                                        | ranges::views::join( '.' )
                                        | ranges::to< std::string >();
                    if( km.component_store().is_initialized( com_name ) )
                    {
                        com_matches.emplace_back( outlet );
                    }
                }
            }
            else
            {
                com_matches.emplace_back( outlet );
            }
        }

        return com_matches;
    }();
    auto executed = UuidSet{};

    for( auto const& outlet : outlets )
    {

        // Ensure all found outlets have corresponding transition states.
        if( !transition_states_.contains( outlet ) )
        {
            KTRY( reset_transitions( outlet ) );
        }
        if( is_active_outlet( outlet ) )
        {
            if( auto const actionn = view::make( outlet ) 
                                   | view::child( "action" )
                                   | view::fetch_node( km )
              ; actionn )
            {
                KTRY( execute_body( actionn.value() ) );
                KTRY( reset_transitions( outlet ) ); // Controversial. Not sure if I want the design to reset after executing an action, but permissible for now.

                executed = KTRY( fetch_outlet_tree( outlet ) );
            }
        }
    }
    // Transitions must occur only after all actions have been accessed. 
    // If they happen interleaved, incorrect transition to action then execute may occur.
    for( auto const& outlet : outlets
                            | ranges::views::remove_if( [ &executed ]( auto const& e ){ return executed.contains( e ); } ) )
    {
        if( is_active_outlet( outlet ) )
        {
            if( auto const actionn = view::make( outlet ) 
                                    | view::child( "action" )
                                    | view::fetch_node( km )
            ; !actionn )
            {
                auto const children = nw->fetch_children( outlet );
                transition_states_.at( outlet )->active = children
                                                        | ranges::views::filter( [ & ]( auto const& e ){ return is_outlet( e ); } )
                                                        | ranges::to< UuidSet >();
            }
        }
    }

    rv = outcome::success();

    return rv;
}

SCENARIO( "EventStore::fire_event", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );

    GIVEN( "one level outlet" )
    {
        REQUIRE_RES( estore->install_subject( "victor" ) );
        REQUIRE_RES( estore->install_verb( "charlie" ) );
        REQUIRE_RES( estore->install_object( "delta" ) );

        auto const ores = estore->install_outlet( Leaf{ .heading = "1"
                                                      , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                      , .description = "test to create child node on event"
                                                      , .action = "kmap.create_child( kmap.root_node(), '1_victor' );" } );
        REQUIRE_RES( ores );

        WHEN( "fire_event" )
        {
            REQUIRE_RES( estore->fire_event( { "subject.victor", "verb.charlie", "object.delta" } ) );

            REQUIRE(( view::make( kmap.root_node_id() ) | view::child( "1_victor" ) | view::exists( kmap ) ));
        }
    }
    GIVEN( "two level outlet with identical requisites" )
    {
        REQUIRE_RES( estore->install_subject( "victor" ) );
        REQUIRE_RES( estore->install_verb( "charlie" ) );
        REQUIRE_RES( estore->install_object( "delta" ) );
        REQUIRE_RES( estore->install_object( "echo" ) );

        auto const ores = estore->install_outlet( Branch{ .heading = "1"
                                                       , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                       , .transitions = { Leaf{ .heading = "1"
                                                                              , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                                              , .description = ""
                                                                              , .action = "kmap.create_child( kmap.root_node(), '1_victor' );" } } } );

        REQUIRE_RES( ores );

        REQUIRE( estore->is_active_outlet( ores.value() ) );

        WHEN( "fire first invalid event" )
        {
            REQUIRE_RES( estore->fire_event( {  "subject.victor", "verb.charlie", "object.echo" } ) );

            THEN( "no change from initial state" )
            {
                REQUIRE( estore->is_active_outlet( ores.value() ) );
            }
        }
        WHEN( "fire first valid event" )
        {
            REQUIRE_RES( estore->fire_event( {  "subject.victor", "verb.charlie", "object.delta" } ) );

            auto const eroot = REQUIRE_TRY( estore->event_root() );
            auto const level2 = REQUIRE_TRY( view::make( eroot )
                                           | view::direct_desc( "outlet.1.1" )
                                           | view::fetch_node( kmap ) );

            THEN( "transition occurred" )
            {
                REQUIRE( estore->is_active_outlet( level2 ) );
            }

            WHEN( "fire second invalid event" )
            {
                REQUIRE_RES( estore->fire_event( {  "subject.victor", "verb.charlie", "object.echo" } ) );

                THEN( "no change from secondary state" )
                {
                    REQUIRE( estore->is_active_outlet( level2 ) );
                }
            }
            WHEN( "fire second valid event" )
            {
                REQUIRE_RES( estore->fire_event( { "subject.victor", "verb.charlie", "object.delta" } ) );

                THEN( "event output found" )
                {
                    REQUIRE(( view::make( kmap.root_node_id() )
                            | view::child( "1_victor" )
                            | view::exists( kmap ) ));
                }
                THEN( "outlet reset" )
                {
                    REQUIRE( estore->is_active_outlet( ores.value() ) );
                }
            }
        }
    }
    GIVEN( "two level outlet with different requisites" )
    {
        REQUIRE_RES( estore->install_subject( "victor" ) );
        REQUIRE_RES( estore->install_verb( "charlie" ) );
        REQUIRE_RES( estore->install_object( "delta" ) );
        REQUIRE_RES( estore->install_object( "echo" ) );

        auto const ores = estore->install_outlet( Branch{ .heading = "1"
                                                       , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                       , .transitions = { Leaf{ .heading = "1"
                                                                              , .requisites = { "subject.victor", "verb.charlie", "object.echo" }
                                                                              , .description = ""
                                                                              , .action = "kmap.create_child( kmap.root_node(), '1_echo' );" } } } );

        REQUIRE_RES( ores );

        REQUIRE( estore->is_active_outlet( ores.value() ) );

        WHEN( "fire first event with first requisites" )
        {
            REQUIRE_RES( estore->fire_event( {  "subject.victor", "verb.charlie", "object.delta" } ) );

            auto const eroot = REQUIRE_TRY( estore->event_root() );
            auto const level2 = REQUIRE_TRY( view::make( eroot )
                                           | view::direct_desc( "outlet.1.1" )
                                           | view::fetch_node( kmap ) );

            THEN( "transition occurred" )
            {
                REQUIRE( estore->is_active_outlet( level2 ) );
            }

            WHEN( "fire second event with second requisites" )
            {
                REQUIRE_RES( estore->fire_event( { "subject.victor", "verb.charlie", "object.echo" } ) );

                THEN( "event output found" )
                {
                    REQUIRE(( view::make( kmap.root_node_id() )
                            | view::child( "1_echo" )
                            | view::exists( kmap ) ));
                }
                THEN( "outlet reset" )
                {
                    REQUIRE( estore->is_active_outlet( ores.value() ) );
                }
            }
            WHEN( "fire second event with first requisites" )
            {
                REQUIRE_RES( estore->fire_event( {  "subject.victor", "verb.charlie", "object.delta" } ) );

                THEN( "second state unchanged" )
                {
                    REQUIRE( estore->is_active_outlet( level2 ) );
                }
            }
        }
        WHEN( "fire first event with second requisites" )
        {
            REQUIRE_RES( estore->fire_event( {  "subject.victor", "verb.charlie", "object.echo" } ) );

            THEN( "initial state unchanged" )
            {
                REQUIRE( estore->is_active_outlet( ores.value() ) );
            }
        }
    }
    GIVEN( "one level outlet with com requirement" )
    {
        auto const nwn = com::Network::id;

        REQUIRE_RES( estore->install_subject( "victor" ) );
        REQUIRE_RES( estore->install_verb( "charlie" ) );
        REQUIRE_RES( estore->install_object( "delta" ) );
        REQUIRE_RES( estore->install_component( nwn ) );

        auto const ores = estore->install_outlet( Leaf{ .heading = "1"
                                                      , .requisites = { "subject.victor", "verb.charlie", "object.delta", fmt::format( "component.{}", nwn ) }
                                                      , .description = "test to create child node on event"
                                                      , .action = "kmap.create_child( kmap.root_node(), '1_victor' );" } );
        REQUIRE_RES( ores );

        WHEN( "fire_event" )
        {
            REQUIRE_RES( estore->fire_event( { "subject.victor", "verb.charlie", "object.delta" } ) );

            THEN( "outlet is triggered because component exists" )
            {
                REQUIRE(( view::make( kmap.root_node_id() ) | view::child( "1_victor" ) | view::exists( kmap ) ));
            }
        }
    }
    GIVEN( "one level outlet with nonexistent com requirement" )
    {
        auto const comn = "nonexistent.test_component";

        REQUIRE_RES( estore->install_subject( "victor" ) );
        REQUIRE_RES( estore->install_verb( "charlie" ) );
        REQUIRE_RES( estore->install_object( "delta" ) );
        REQUIRE_RES( estore->install_component( comn ) );

        auto const ores = estore->install_outlet( Leaf{ .heading = "1"
                                                      , .requisites = { "subject.victor", "verb.charlie", "object.delta", fmt::format( "component.{}", comn ) }
                                                      , .description = "test to create child node on event"
                                                      , .action = "kmap.create_child( kmap.root_node(), '1_victor' );" } );
        REQUIRE_RES( ores );

        WHEN( "fire_event" )
        {
            REQUIRE_RES( estore->fire_event( { "subject.victor", "verb.charlie", "object.delta" } ) );

            THEN( "outlet not triggered because component doesn't exist" )
            {
                REQUIRE( !( view::make( kmap.root_node_id() ) | view::child( "1_victor" ) | view::exists( kmap ) ) );
            }
        }
    }
}

auto EventStore::is_active_outlet( Uuid const& outlet )
    -> bool
{
    if( transition_states_.contains( outlet ) )
    {
        return transition_states_.at( outlet )->active.contains( outlet );
    }
    else
    {
        return false;
    }
}

auto EventStore::is_outlet( Uuid const& node )
    -> bool
{
    return view::make( node )
         | view::child( "requisite" ) // TODO: Could be more rigourous with this i.e., child( all_of( "requisite", any_of( "action", ...sub_outlet... ) ) )
         | view::child/*( ...alias... )*/ // TODO: Improve by requiring the requisite child points to outlet.[subject|verb|object]
         | view::exists( kmap_inst() );
}

auto EventStore::reset_transitions( Uuid const& outlet )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                // TODO: outlet tree all points to same transition state.
            }
        })
    ;

    KMAP_ENSURE( is_outlet( outlet ), error_code::network::invalid_node );

    auto const ofront = KTRY( fetch_outlet_base( outlet ) );
    auto const otree = KTRY( fetch_outlet_tree( outlet ) );

    BC_ASSERT( !otree.empty() ); // Tree should, at least, include `outlet` itself.

    auto shared_state = std::make_shared< TransitionState >( TransitionState{ .active = { ofront } } );
    // Fill remaining sub-outlets with same active state.
    for( auto const& ao : otree )
    {
        transition_states_[ ao ] = shared_state;
    }

    rv = outcome::success();
    
    return rv;
}

SCENARIO( "EventStore::reset_transitions( Uuid )", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );

    GIVEN( "one (very) depressed key" )
    {
        REQUIRE_RES( estore->install_subject( "network" ) );
        REQUIRE_RES( estore->install_verb( "depressed" ) );
        REQUIRE_RES( estore->install_object( "keyboard.key.g" ) );
        REQUIRE_RES( estore->install_outlet( Branch{ .heading = "network.g"
                                                  , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                  , .transitions = { Leaf{ .heading = "g"
                                                                         , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                                         , .description = "travel to top sibling."
                                                                         , .action = R"%%%(/*do nothing*/)%%%" } } } ) );

        auto const eroot = REQUIRE_TRY( estore->event_root() );
        auto const branchn = REQUIRE_TRY( view::make( eroot )
                                        | view::direct_desc( "outlet.network.g" )
                                        | view::fetch_node( kmap ) );
        auto const leafn = REQUIRE_TRY( view::make( eroot )
                                      | view::direct_desc( "outlet.network.g.g" )
                                      | view::fetch_node( kmap ) );

        REQUIRE( estore->is_active_outlet( branchn ) );
        REQUIRE( !estore->is_active_outlet( leafn ) );

        WHEN( "reset branch node transitions" )
        {
            REQUIRE_RES( estore->reset_transitions( branchn ) );

            THEN( "transitions unchanged" )
            {
                REQUIRE( estore->is_active_outlet( branchn ) );
                REQUIRE( !estore->is_active_outlet( leafn ) );
            }
        }
        WHEN( "reset leaf node transitions" )
        {
            REQUIRE_RES( estore->reset_transitions( leafn ) );

            THEN( "transitions unchanged" )
            {
                REQUIRE( estore->is_active_outlet( branchn ) );
                REQUIRE( !estore->is_active_outlet( leafn ) );
            }
        }
        WHEN( "transition occurs" )
        {
            REQUIRE_RES( estore->fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.g" } ) );

            THEN( "transitioned from branch to leaf" )
            {
                REQUIRE( !estore->is_active_outlet( branchn ) );
                REQUIRE( estore->is_active_outlet( leafn ) );
            }

            WHEN( "reset branch node transitions" )
            {
                REQUIRE_RES( estore->reset_transitions( branchn ) );

                THEN( "transitions reset" )
                {
                    REQUIRE( estore->is_active_outlet( branchn ) );
                    REQUIRE( !estore->is_active_outlet( leafn ) );
                }
            }
            WHEN( "reset leaf node transitions" )
            {
                REQUIRE_RES( estore->reset_transitions( leafn ) );

                THEN( "transitions reset" )
                {
                    REQUIRE( estore->is_active_outlet( branchn ) );
                    REQUIRE( !estore->is_active_outlet( leafn ) );
                }
            }
        }
    }
}

auto EventStore::reset_transitions( std::set< std::string > const& requisites )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                // TODO: outlet tree all points to same transition state.
            }
        })
    ;

    auto matches = UuidSet{};
    auto const eroot = KTRY( event_root() );
    auto const req_ns = view::make( eroot )
                      | view::direct_desc( view::all_of( requisites ) )
                      | view::to_node_set( km );
    auto const outlet_ns = view::make( eroot )
                         | view::desc
                         | view::child( "requisite" )
                         | view::parent
                         | view::to_node_set( km );

    for( auto const& outlet : outlet_ns )
    {
        auto const oreqs = view::make( outlet )
                         | view::child( "requisite" )
                         | view::child
                         | view::resolve
                         | view::to_node_set( km );
        auto const pred = [ & ]( auto const& oreq )
        {
            // TODO: Unit test, of course, but also, I think we can do the same for fire_event, but in the other direction:
            //       is_lineal( oreq, tret );
            // Matches descendants as well e.g.:
            //     reset_transitions( { "subject.keyboard.key" } )
            //     matches:
            //         outlet.<T>.requisite.key[subject.keyboard.key]
            //         outlet.<T>.requisite.key[subject.keyboard.key.g]
            //     but not:
            //         outlet.<T>.requisite.key[subject.keyboard]
            return ranges::any_of( req_ns, [ & ]( auto const& treq ){ return nw->is_lineal( treq, oreq ); } );
        };

        // Match all `outlet.requisite`s to provided requisites.
        if( ranges::all_of( oreqs, pred ) )
        {
            matches.emplace( outlet );
        }
    }

    for( auto const& match : matches )
    {
        KTRY( reset_transitions( match ) );
    }

    rv = outcome::success();
    
    return rv;
}

SCENARIO( "EventStore::reset_transitions( requisites )", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );

    GIVEN( "one (very) depressed key" )
    {
        REQUIRE_RES( estore->install_subject( "network" ) );
        REQUIRE_RES( estore->install_verb( "depressed" ) );
        REQUIRE_RES( estore->install_object( "keyboard.key.g" ) );
        REQUIRE_RES( estore->install_outlet( Branch{ .heading = "network.g"
                                                  , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                  , .transitions = { Leaf{ .heading = "g"
                                                                         , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                                         , .description = "travel to top sibling."
                                                                         , .action = R"%%%(/*do nothing*/)%%%" } } } ) );

        auto const eroot = REQUIRE_TRY( estore->event_root() );
        auto const branchn = REQUIRE_TRY( view::make( eroot )
                                        | view::direct_desc( "outlet.network.g" )
                                        | view::fetch_node( kmap ) );
        auto const leafn = REQUIRE_TRY( view::make( eroot )
                                      | view::direct_desc( "outlet.network.g.g" )
                                      | view::fetch_node( kmap ) );

        REQUIRE( estore->is_active_outlet( branchn ) );
        REQUIRE( !estore->is_active_outlet( leafn ) );

        WHEN( "reset transitions" )
        {
            REQUIRE_RES( estore->reset_transitions( { "subject.network", "verb.depressed", "object.keyboard.key" } ) );

            THEN( "active state remains unchanged" )
            {
                REQUIRE( estore->is_active_outlet( branchn ) );
                REQUIRE( !estore->is_active_outlet( leafn ) );
            }
        }
        WHEN( "transition occurs" )
        {
            REQUIRE_RES( estore->fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.g" } ) );

            THEN( "transitioned from branch to leaf" )
            {
                REQUIRE( !estore->is_active_outlet( branchn ) );
                REQUIRE( estore->is_active_outlet( leafn ) );
            }

            WHEN( "reset transitions" )
            {
                REQUIRE_RES( estore->reset_transitions( { "subject.network", "verb.depressed", "object.keyboard.key" } ) );

                THEN( "transitons reset" )
                {
                    REQUIRE( estore->is_active_outlet( branchn ) );
                    REQUIRE( !estore->is_active_outlet( leafn ) );
                }
            }
        }
    }
}

namespace binding {

using namespace emscripten;

struct EventStore
{
    Kmap& kmap_;

    EventStore( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto fire_event( std::vector< std::string > const& requisites )
        -> kmap::binding::Result< void >
    {
        auto const estore = KTRY( kmap_.fetch_component< com::EventStore >() );

        return estore->fire_event( requisites | ranges::to< std::set >() );
    }
    auto reset_transitions( std::vector< std::string > const& requisites )
        -> kmap::binding::Result< void >
    {
        auto const estore = KTRY( kmap_.fetch_component< com::EventStore >() );

        return estore->reset_transitions( requisites | ranges::to< std::set >() );
    }
};

auto event_store()
    -> binding::EventStore
{
    return binding::EventStore{ Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_event_store )
{
    function( "event_store", &kmap::com::binding::event_store );
    class_< kmap::com::binding::EventStore >( "EventStore" )
        .function( "fire_event", &kmap::com::binding::EventStore::fire_event )
        .function( "reset_transitions", &kmap::com::binding::EventStore::reset_transitions )
        ;
}

} // namespace binding

namespace {
namespace event_store_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::EventStore
,   std::set({ "command_store"s, "root_node"s })
,   "event related functionality"
);

} // namespace event_store_def 
}

} // namespace kmap::com