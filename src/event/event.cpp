/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "event.hpp"

#include "cmd/parser.hpp"
#include "error/network.hpp"
#include "error/parser.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/front.hpp"
#include "path/act/order.hpp"
#include "path/act/push.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove_if.hpp>

#include <vector>

namespace kmap {

EventStore::EventStore( Kmap& kmap )
    : kmap_{ kmap }
{
}

auto EventStore::install_defaults()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    // {
    //     auto const keyboard = KMAP_TRY( install_event_action( "keyboard"
    //                                                         , "fires keyboard actions"
    //                                                         , ""
    //                                                         , "" ) );
    //     At timeout: estore.reset_transitions( { "object.keyboard" } );
    // }
    // object.canvas.network
    // object.
    // What this does: setTimeout( fn(){ kmap.fire_event( "clear.keyboard.keys" ) } )

    rv = outcome::success();

    return rv;
}

auto EventStore::action( std::string const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    
    rv = KMAP_TRY( view::make( event_root() )
                 | view::child( "action" )
                 | view::desc( heading )
                 | view::single
                 | view::create_node( kmap_ )
                 | view::to_single );

    return rv;
}

auto EventStore::object( std::string const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    
    rv = KMAP_TRY( view::make( event_root() )
                 | view::child( "object" )
                 | view::desc( heading )
                 | view::single
                 | view::create_node( kmap_ )
                 | view::to_single );

    return rv;
}

auto EventStore::event_root()
    -> Uuid
{
    auto const mroot = kmap_.root_node_id();

    return KMAP_TRYE( kmap_.fetch_or_create_descendant( mroot, mroot, "/meta.event" ) );
}

auto EventStore::event_root() const
    -> Result< Uuid >
{
    return KMAP_TRY( kmap_.fetch_descendant( "/meta.event" ) );
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

    auto const ofront = KTRY( view::make( event_root() )
                            | view::child( "outlet" )
                            | view::rlineage( node )
                            | view::child( "requisite" )
                            | view::child/*( TODO: aliases to any subject/verb/object )*/
                            | view::parent
                            | view::parent
                            | view::to_node_set( kmap_ )
                            | ranges::to< std::vector >()
                            | act::order( kmap_ )
                            | act::front );

    rv = ofront;

    return rv;
}

auto EventStore::fetch_outlet_tree( Uuid const& outlet )
    -> Result< UuidSet >
{
    KMAP_ENSURE( is_outlet( outlet ), error_code::network::invalid_node );

    auto rv = KMAP_MAKE_RESULT( UuidSet );
    auto const obase = KTRY( fetch_outlet_base( outlet ) );
    auto const otree = view::make( obase )
                     | view::desc
                     | view::child( "requisite" )
                     | view::parent
                     | view::to_node_set( kmap_ )
                     | act::push( obase );

    rv = otree;

    return rv;
}

auto EventStore::install_verb( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const root = event_root();

    rv = view::make( root )
       | view::child( "verb" )
       | view::direct_desc( heading ) 
       | view::fetch_or_create_node( kmap_ ); // TODO: For all of these install_* fns, I'm doing a fetch_or_create for convenience, but the name "install" implies that it doesn't already exist. Either rename (ensure_subject_installed?) or return error.

    return rv;
}

auto EventStore::install_object( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const eroot = event_root();

    rv = KMAP_TRY( view::make( eroot )
                 | view::child( "object" )
                 | view::direct_desc( heading )
                 | view::fetch_or_create_node( kmap_ ) );

    return rv;
}

auto EventStore::install_subject( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const eroot = event_root();

    rv = KMAP_TRY( view::make( eroot )
                 | view::child( "subject" )
                 | view::desc( fmt::format( ".{}", heading ) )
                 | view::fetch_or_create_node( kmap_ ) );

    return rv;
}

auto EventStore::install_outlet( Leaf const& leaf )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const eroot = event_root();
    auto const oroot = KMAP_TRY( view::make( eroot )
                               | view::child( "outlet" )
                               | view::desc( fmt::format( ".{}", leaf.heading ) )
                               | view::create_node( kmap_ )
                               | view::to_single );

    KTRY( install_outlet_internal( oroot, leaf ) );
    KTRY( reset_transitions( oroot ) );

    rv = oroot;

    return rv;
}

SCENARIO( "install_outlet", "[event]" )
{
    KMAP_EVENT_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();

    GIVEN( "requisites" )
    {
        auto& estore = kmap.event_store();

        REQUIRE_RES( estore.install_subject( "victor" ) );
        REQUIRE_RES( estore.install_verb( "charlied" ) );
        REQUIRE_RES( estore.install_object( "delta" ) );

        WHEN( "install leaf" )
        {
            auto const leaf = Leaf{ .heading = "network.open_editor"
                                  , .requisites = { "subject.victor", "verb.charlied", "object.delta" }
                                  , .description = "VCD"
                                  , .action = "kmap.create_child(kmap.rood_node(), '1');" };
            auto const oleaf = REQUIRE_TRY( estore.install_outlet( leaf ) );

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
    auto const eroot = event_root();
    auto const oroot = KMAP_TRY( view::make( eroot )
                               | view::child( "outlet" )
                               | view::desc( fmt::format( ".{}", branch.heading ) )
                               | view::create_node( kmap_ )
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

    KMAP_ENSURE( js::lint( leaf.action ), error_code::js::lint_failed );

    auto const action_body = fmt::format( "```javascript\n{}\n```", leaf.action );

    KMAP_ENSURE( cmd::parser::parse_body_code( action_body ), error_code::parser::parse_failed );

    for( auto const& e : leaf.requisites )
    {
        KMAP_TRY( view::make( root )
                | view::child( "requisite" ) 
                | view::alias( view::make( event_root() ) | view::desc( e ) ) 
                | view::create_node( kmap_ ) );
    }

    auto const descn  = KMAP_TRY( view::make( root )
                                | view::child( "description" ) 
                                | view::single
                                | view::create_node( kmap_ )
                                | view::to_single );
    auto const actionn = KMAP_TRY( view::make( root )
                                 | view::child( "action" ) 
                                 | view::single
                                 | view::create_node( kmap_ )
                                 | view::to_single );

    KMAP_TRY( kmap_.update_body( descn, leaf.description ) );
    KMAP_TRY( kmap_.update_body( actionn, action_body ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::install_outlet_internal( Uuid const& root 
                                        , Branch const& branch )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( !branch.transitions.empty(), error_code::common::uncategorized );

    for( auto const& e : branch.requisites )
    {
        KMAP_TRY( view::make( root )
                | view::child( "requisite" ) 
                | view::alias( view::make( event_root() ) | view::desc( e ) ) 
                | view::create_node( kmap_ ) );
    }

    for( auto const& transition : branch.transitions )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( Leaf const& leaf ) -> Result< void >
            {
                auto const lroot = KTRY( view::make( root )
                                       | view::child( leaf.heading )
                                       | view::create_node( kmap_ )
                                       | view::to_single );

                return install_outlet_internal( lroot, leaf );
            }
        ,   [ & ]( Branch const& branch ) -> Result< void >
            {
                auto const broot = KTRY( view::make( root )
                                       | view::child( branch.heading )
                                       | view::create_node( kmap_ )
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
    auto const eroot = event_root();

    KMAP_TRY( view::make( eroot )
            | view::child( "subject" )
            | view::direct_desc( heading )
            | view::erase_node( kmap_ ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_subject( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( kmap_.erase_node( node ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_verb( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const eroot = event_root();

    KMAP_TRY( view::make( eroot )
            | view::child( "verb" )
            | view::direct_desc( heading )
            | view::erase_node( kmap_ ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_verb( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( kmap_.erase_node( node ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_object( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const eroot = event_root();

    KMAP_TRY( view::make( eroot )
            | view::child( "object" )
            | view::direct_desc( heading )
            | view::erase_node( kmap_ ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_object( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( kmap_.erase_node( node ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_outlet( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const eroot = event_root();

    KMAP_TRY( view::make( eroot )
            | view::child( "outlet" )
            | view::direct_desc( heading )
            | view::erase_node( kmap_ ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_outlet( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( kmap_.erase_node( node ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::uninstall_outlet_transition( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( kmap_.erase_node( node ) );

    rv = outcome::success();

    return rv;
}

auto EventStore::execute_body( Uuid const& node )
    -> Result< void >
{    
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( kmap_.exists( node ), error_code::network::invalid_node );

    auto const body = KMAP_TRY( kmap_.fetch_body( node ) );
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

// TODO: If the requested subject, verb, or object is missing, don't err, it just means one hasn't been installed yet, so there'll be nothing to match against.
auto EventStore::fire_event( std::set< std::string > const& requisites )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const eroot = event_root();
    auto const oroot = KTRY( view::make( eroot )
                           | view::child( "object" )
                           | view::fetch_node( kmap_ ) );
    auto const objects = requisites
                       | ranges::views::filter( []( auto const& e ){ return e.starts_with( "object" ); } )
                       | ranges::to< std::set >();
    auto const others = requisites
                      | ranges::views::filter( []( auto const& e ){ return !e.starts_with( "object" ); } )
                      | ranges::to< std::set >();

    KMAP_ENSURE_MSG( objects.size() == 1, error_code::common::uncategorized, "currently limiting object to a single entry (no conceptual limitation, just ease of impl." );

    for( auto const& opath : objects )
    {
        auto node = KMAP_TRY( view::make( eroot )
                            | view::desc( opath )
                            | view::fetch_node( kmap_ ) );
        
        while( oroot != node )
        {
            auto combined = others;
            auto const abspath = kmap_.absolute_path( oroot, node );

            combined.emplace( abspath | ranges::views::join( '.' ) | ranges::to< std::string >() );

            KTRY( fire_event_internal( combined ) );

            node = KTRY( kmap_.fetch_parent( node ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto EventStore::fire_event_internal( std::set< std::string > const& requisites )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const ver = view::make( event_root() );

    auto const vreqs = ver | view::direct_desc( view::all_of( requisites ) );
    auto const vor = ver | view::child( "outlet" );
    auto const outlets = vor
                       | view::desc( "requisite" )
                       | view::child( view::all_of( vreqs | view::to_heading_set( kmap_ ) ) ) // TODO: Actually needs view::alias( reqs ), rather than just the heading matches.
                       | view::parent
                       | view::parent
                       | view::to_node_set( kmap_ )
                       | ranges::to< std::vector >()
                       | act::order( kmap_ );
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
                                   | view::fetch_node( kmap_)
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
                                    | view::fetch_node( kmap_)
            ; !actionn )
            {
                auto const children = kmap_.fetch_children( outlet );
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
    KMAP_EVENT_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto& estore = kmap.event_store();

    GIVEN( "one level outlet" )
    {
        REQUIRE_RES( estore.install_subject( "victor" ) );
        REQUIRE_RES( estore.install_verb( "charlie" ) );
        REQUIRE_RES( estore.install_object( "delta" ) );

        auto const ores = estore.install_outlet( Leaf{ .heading = "1"
                                                     , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                     , .description = ""
                                                     , .action = "kmap.create_child( kmap.root_node(), '1_victor' );" } );
        REQUIRE_RES( ores );

        WHEN( "fire_event" )
        {
            REQUIRE_RES( estore.fire_event( { "subject.victor", "verb.charlie", "object.delta" } ) );

            REQUIRE(( view::make( kmap.root_node_id() ) | view::child( "1_victor" ) | view::exists( kmap ) ));
        }
    }
    GIVEN( "two level outlet with identical requisites" )
    {
        REQUIRE_RES( estore.install_subject( "victor" ) );
        REQUIRE_RES( estore.install_verb( "charlie" ) );
        REQUIRE_RES( estore.install_object( "delta" ) );
        REQUIRE_RES( estore.install_object( "echo" ) );

        auto const ores = estore.install_outlet( Branch{ .heading = "1"
                                                       , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                       , .transitions = { Leaf{ .heading = "1"
                                                                              , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                                              , .description = ""
                                                                              , .action = "kmap.create_child( kmap.root_node(), '1_victor' );" } } } );

        REQUIRE_RES( ores );

        REQUIRE( estore.is_active_outlet( ores.value() ) );

        WHEN( "fire first invalid event" )
        {
            REQUIRE_RES( estore.fire_event( {  "subject.victor", "verb.charlie", "object.echo" } ) );

            THEN( "no change from initial state" )
            {
                REQUIRE( estore.is_active_outlet( ores.value() ) );
            }
        }
        WHEN( "fire first valid event" )
        {
            REQUIRE_RES( estore.fire_event( {  "subject.victor", "verb.charlie", "object.delta" } ) );

            auto const level2 = REQUIRE_TRY( view::make( estore.event_root() )
                                           | view::direct_desc( "outlet.1.1" )
                                           | view::fetch_node( kmap ) );

            THEN( "transition occurred" )
            {
                REQUIRE( estore.is_active_outlet( level2 ) );
            }

            WHEN( "fire second invalid event" )
            {
                REQUIRE_RES( estore.fire_event( {  "subject.victor", "verb.charlie", "object.echo" } ) );

                THEN( "no change from secondary state" )
                {
                    REQUIRE( estore.is_active_outlet( level2 ) );
                }
            }
            WHEN( "fire second valid event" )
            {
                REQUIRE_RES( estore.fire_event( { "subject.victor", "verb.charlie", "object.delta" } ) );

                THEN( "event output found" )
                {
                    REQUIRE(( view::make( kmap.root_node_id() )
                            | view::child( "1_victor" )
                            | view::exists( kmap ) ));
                }
                THEN( "outlet reset" )
                {
                    REQUIRE( estore.is_active_outlet( ores.value() ) );
                }
            }
        }
    }
    GIVEN( "two level outlet with different requisites" )
    {
        REQUIRE_RES( estore.install_subject( "victor" ) );
        REQUIRE_RES( estore.install_verb( "charlie" ) );
        REQUIRE_RES( estore.install_object( "delta" ) );
        REQUIRE_RES( estore.install_object( "echo" ) );

        auto const ores = estore.install_outlet( Branch{ .heading = "1"
                                                       , .requisites = { "subject.victor", "verb.charlie", "object.delta" }
                                                       , .transitions = { Leaf{ .heading = "1"
                                                                              , .requisites = { "subject.victor", "verb.charlie", "object.echo" }
                                                                              , .description = ""
                                                                              , .action = "kmap.create_child( kmap.root_node(), '1_echo' );" } } } );

        REQUIRE_RES( ores );

        REQUIRE( estore.is_active_outlet( ores.value() ) );

        WHEN( "fire first event with first requisites" )
        {
            REQUIRE_RES( estore.fire_event( {  "subject.victor", "verb.charlie", "object.delta" } ) );

            auto const level2 = REQUIRE_TRY( view::make( estore.event_root() )
                                        | view::direct_desc( "outlet.1.1" )
                                        | view::fetch_node( kmap ) );

            THEN( "transition occurred" )
            {
                REQUIRE( estore.is_active_outlet( level2 ) );
            }

            WHEN( "fire second event with second requisites" )
            {
                REQUIRE_RES( estore.fire_event( { "subject.victor", "verb.charlie", "object.echo" } ) );

                THEN( "event output found" )
                {
                    REQUIRE(( view::make( kmap.root_node_id() )
                            | view::child( "1_echo" )
                            | view::exists( kmap ) ));
                }
                THEN( "outlet reset" )
                {
                    REQUIRE( estore.is_active_outlet( ores.value() ) );
                }
            }
            WHEN( "fire second event with first requisites" )
            {
                REQUIRE_RES( estore.fire_event( {  "subject.victor", "verb.charlie", "object.delta" } ) );

                THEN( "second state unchanged" )
                {
                    REQUIRE( estore.is_active_outlet( level2 ) );
                }
            }
        }
        WHEN( "fire first event with second requisites" )
        {
            REQUIRE_RES( estore.fire_event( {  "subject.victor", "verb.charlie", "object.echo" } ) );

            THEN( "initial state unchanged" )
            {
                REQUIRE( estore.is_active_outlet( ores.value() ) );
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
         | view::exists( kmap_ );
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
    KMAP_EVENT_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto& estore = kmap.event_store();

    GIVEN( "one (very) depressed key" )
    {
        REQUIRE_RES( estore.install_subject( "network" ) );
        REQUIRE_RES( estore.install_verb( "depressed" ) );
        REQUIRE_RES( estore.install_object( "keyboard.key.g" ) );
        REQUIRE_RES( estore.install_outlet( Branch{ .heading = "network.g"
                                                  , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                  , .transitions = { Leaf{ .heading = "g"
                                                                         , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                                         , .description = "travel to top sibling."
                                                                         , .action = R"%%%(/*do nothing*/)%%%" } } } ) );

        auto const branchn = REQUIRE_TRY( view::make( estore.event_root() )
                                        | view::direct_desc( "outlet.network.g" )
                                        | view::fetch_node( kmap ) );
        auto const leafn = REQUIRE_TRY( view::make( estore.event_root() )
                                      | view::direct_desc( "outlet.network.g.g" )
                                      | view::fetch_node( kmap ) );

        REQUIRE( estore.is_active_outlet( branchn ) );
        REQUIRE( !estore.is_active_outlet( leafn ) );

        WHEN( "reset branch node transitions" )
        {
            REQUIRE_RES( estore.reset_transitions( branchn ) );

            THEN( "transitions unchanged" )
            {
                REQUIRE( estore.is_active_outlet( branchn ) );
                REQUIRE( !estore.is_active_outlet( leafn ) );
            }
        }
        WHEN( "reset leaf node transitions" )
        {
            REQUIRE_RES( estore.reset_transitions( leafn ) );

            THEN( "transitions unchanged" )
            {
                REQUIRE( estore.is_active_outlet( branchn ) );
                REQUIRE( !estore.is_active_outlet( leafn ) );
            }
        }
        WHEN( "transition occurs" )
        {
            REQUIRE_RES( estore.fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.g" } ) );

            THEN( "transitioned from branch to leaf" )
            {
                REQUIRE( !estore.is_active_outlet( branchn ) );
                REQUIRE( estore.is_active_outlet( leafn ) );
            }

            WHEN( "reset branch node transitions" )
            {
                REQUIRE_RES( estore.reset_transitions( branchn ) );

                THEN( "transitions reset" )
                {
                    REQUIRE( estore.is_active_outlet( branchn ) );
                    REQUIRE( !estore.is_active_outlet( leafn ) );
                }
            }
            WHEN( "reset leaf node transitions" )
            {
                REQUIRE_RES( estore.reset_transitions( leafn ) );

                THEN( "transitions reset" )
                {
                    REQUIRE( estore.is_active_outlet( branchn ) );
                    REQUIRE( !estore.is_active_outlet( leafn ) );
                }
            }
        }
    }
}

auto EventStore::reset_transitions( std::set< std::string > const& requisites )
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

    auto matches = UuidSet{};
    auto const req_ns = view::make( event_root() )
                      | view::direct_desc( view::all_of( requisites ) )
                      | view::to_node_set( kmap_ );
    auto const outlet_ns = view::make( event_root() )
                         | view::desc
                         | view::child( "requisite" )
                         | view::parent
                         | view::to_node_set( kmap_ );

    for( auto const& outlet : outlet_ns )
    {
        auto const oreqs = view::make( outlet )
                         | view::child( "requisite" )
                         | view::child
                         | view::resolve
                         | view::to_node_set( kmap_);
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
            return ranges::any_of( req_ns, [ & ]( auto const& treq ){ return kmap_.is_lineal( treq, oreq ); } );
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
    KMAP_EVENT_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto& estore = kmap.event_store();

    GIVEN( "one (very) depressed key" )
    {
        REQUIRE_RES( estore.install_subject( "network" ) );
        REQUIRE_RES( estore.install_verb( "depressed" ) );
        REQUIRE_RES( estore.install_object( "keyboard.key.g" ) );
        REQUIRE_RES( estore.install_outlet( Branch{ .heading = "network.g"
                                                  , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                  , .transitions = { Leaf{ .heading = "g"
                                                                         , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.g" }
                                                                         , .description = "travel to top sibling."
                                                                         , .action = R"%%%(/*do nothing*/)%%%" } } } ) );

        auto const branchn = REQUIRE_TRY( view::make( estore.event_root() )
                                        | view::direct_desc( "outlet.network.g" )
                                        | view::fetch_node( kmap ) );
        auto const leafn = REQUIRE_TRY( view::make( estore.event_root() )
                                      | view::direct_desc( "outlet.network.g.g" )
                                      | view::fetch_node( kmap ) );

        REQUIRE( estore.is_active_outlet( branchn ) );
        REQUIRE( !estore.is_active_outlet( leafn ) );

        WHEN( "reset transitions" )
        {
            REQUIRE_RES( estore.reset_transitions( { "subject.network", "verb.depressed", "object.keyboard.key" } ) );

            THEN( "active state remains unchanged" )
            {
                REQUIRE( estore.is_active_outlet( branchn ) );
                REQUIRE( !estore.is_active_outlet( leafn ) );
            }
        }
        WHEN( "transition occurs" )
        {
            REQUIRE_RES( estore.fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.g" } ) );

            THEN( "transitioned from branch to leaf" )
            {
                REQUIRE( !estore.is_active_outlet( branchn ) );
                REQUIRE( estore.is_active_outlet( leafn ) );
            }

            WHEN( "reset transitions" )
            {
                REQUIRE_RES( estore.reset_transitions( { "subject.network", "verb.depressed", "object.keyboard.key" } ) );

                THEN( "transitons reset" )
                {
                    REQUIRE( estore.is_active_outlet( branchn ) );
                    REQUIRE( !estore.is_active_outlet( leafn ) );
                }
            }
        }
    }
}

} // namespace kmap
