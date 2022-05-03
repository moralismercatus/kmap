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
#include "path/node_view.hpp"

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
    // }
    // object.canvas.network
    // object.

    rv = outcome::success();

    return rv;
}

auto EventStore::action( std::string const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    
    rv = KMAP_TRY( view::root( event_root() )
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
    
    rv = KMAP_TRY( view::root( event_root() )
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

// TODO: I don't know where this was headed. Perhaps what is meant here is install_event_listener, rather than event?
//       That would make a little more sense, but I'm not sure how much... naively, I would design the event system thusly:
//       meta.event.listener.keyboard.keydown.j.desciption
//       meta.event.listener.keyboard.keydown.j.active
//       meta.event.listener.keyboard.keydown.j.action
//       meta.event.listener.shutdown.desciption
//       meta.event.listener.shutdown.action
//       But maybe that's the old way of thinking in terms of categories... undecided, but it seems I could proceed this way initially.
//       Meaning... (1) install_event installs the event category e.g., `event.keyboard.keydown.j.action`, I suppose, installs the event to the underlying system, so when an event occurs, it will fire.
//                  (2) install_event_listener installs a listener. Now, there can be multiple listeners to a single event, so...
//                      it seems we'd need a way to make children of the event as listeners with their own [desc,active,action] nodes.
//                      Wouldn't it make better sense to use tagging, for orthogonal listeners, not for event registration.
//                      e.g., [keyboard.keydown.j][keyboard.keydown.shift], to combine events?
//                      ... register_listener( ...any_of( "keyboard.keydown.esc", all_of( "keyboard.keydown.ctrl", "keyboard.keydown.c" )... )
//                      I think that's really the way to go. The events themselves can and should be hierarchical, but the listeners should be able to be orthogonal, across event trees.
//                      And that's fine... but how to represent that as a node system? Aliases to the events, I guess?
//                      meta.event.listener.leave_editor.desciption
//                      meta.event.listener.leave_editor.event.<alias event.keyboard.esc>
//                      meta.event.listener.leave_editor.event.<alias event.keyboard.ctrl> // How to get + c here? The any_of/all_of specification?
//                      or maybe: meta.event.listener.leave_editor.registeration {in body: register_listener( ...any_of( "keyboard.keydown.esc", all_of( "keyboard.keydown.ctrl", "keyboard.keydown.c" )... }
//                      I think ^^^^ might be the solution, combined with a state machine capable receiving fired events, updating event state, and firing listener callbacks.
//                      meta.event.listener.leave_editor.action

//                      But that requires polling aliases...
//                      Further, if events are sequential i.e.: `fire event.keyboard.ctrl` then `fire event.keyboard.c`, how do I relate them?
//                      Well, the way I currently do it is that I don't. I simply wait for the one and ask if the other state is currently true.
//                      I'm not sure that there's really any way to have sequential event firing for all_of(). That would require parallel event firing.
//                      FWIW, in the case of keyboard, the only way to leave the keydown state is through a keyup state, meaning once keydown is fired, it's current
//                      until keyup.
auto EventStore::install_verb( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const root = event_root();

    rv = view::root( root )
       | view::child( "verb" )
       | view::desc( fmt::format( ".{}", heading ) ) 
       | view::fetch_or_create_node( kmap_ ); // TODO: For all of these install_* fns, I'm doing a fetch_or_create for convenience, but the name "install" implies that it doesn't already exist. Either rename (ensure_subject_installed?) or return error.

    return rv;
}

auto EventStore::install_object( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const eroot = event_root();

    rv = KMAP_TRY( view::root( eroot )
                 | view::child( "object" )
                 | view::desc( fmt::format( ".{}", heading ) )
                 | view::fetch_or_create_node( kmap_ ) );

    return rv;
}

auto EventStore::install_subject( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const eroot = event_root();

    rv = KMAP_TRY( view::root( eroot )
                 | view::child( "subject" )
                 | view::desc( fmt::format( ".{}", heading ) )
                 | view::fetch_or_create_node( kmap_ ) );

    return rv;
}
#if 0
auto EventStore::install_source( Heading const& heading
                               , std::string const& desc
                               , std::string const& install )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const root = event_root();
    auto const v = kmap_.make_view( root )
                 | view::child( "source" )
                 | view::desc( fmt::format( ".{}", heading ) );
    auto const descn  = KMAP_TRY( v | view::child( "description" ) | view::create_node( kmap_ ) );
    auto const installn = KMAP_TRY( v | view::child( "installation" ) | view::create_node( kmap_ ) );

    KMAP_TRY( kmap_.update_body( descn, desc ) );
    KMAP_TRY( kmap_.update_body( installn, fmt::format( "```javascript\n{}\n```", install ) ) );

    rv = KMAP_TRY( v | view::fetch_node );

    return rv;
}
#endif //

auto EventStore::install_outlet( std::string const& heading
                               , std::string const& desc
                               , std::string const& action 
                               , std::set< std::string > const& requisites )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const action_body = fmt::format( "```javascript\n{}\n```", action );

    KMAP_ENSURE( cmd::parser::parse_body_code( action_body ), error_code::parser::parse_failed );

    auto const eroot = event_root();
    auto const vr = view::root( eroot )
                  | view::child( "outlet" )
                  | view::desc( fmt::format( ".{}", heading ) );
    auto const descn  = KMAP_TRY( vr 
                                | view::child( "description" ) 
                                | view::single
                                | view::create_node( kmap_ )
                                | view::to_single );
    auto const actionn = KMAP_TRY( vr 
                                 | view::child( "action" ) 
                                 | view::single
                                 | view::create_node( kmap_ )
                                 | view::to_single );

    KMAP_TRY( kmap_.update_body( descn, desc ) );
    KMAP_TRY( kmap_.update_body( actionn, action_body ) );

    for( auto const& e : requisites )
    {
        KMAP_TRY( vr
                | view::child( "requisite" ) 
                | view::alias( view::root( eroot ) | view::desc( e ) ) 
                | view::create_node( kmap_ ) );
    }

    rv = KMAP_TRY( vr | view::fetch_node( kmap_ ) );

    return rv;
}

auto EventStore::uninstall_outlet( std::string const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const eroot = event_root();

    KMAP_TRY( view::root( eroot )
            | view::direct_desc( heading )
            | view::erase_node( kmap_ ) );

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

auto EventStore::fire_event( std::set< std::string > const& requisites )
    -> Result< void >
{
    // for( auto const& e : requisites )
    // {
    //     fmt::print( "\trequisite: {}\n", e );
    // }
    auto rv = KMAP_MAKE_RESULT( void );
    auto const ver = view::root( event_root() );
    auto const alias_reqs = ver | view::direct_desc( view::all_of( requisites ) );
    auto const actions = ver
                       | view::child( "outlet" )
                       | view::desc( "requisite" )
                       | view::child( alias_reqs )
                     // TODO: | view::child( view::all_of( ver | view::direct_desc( view::all_of( requisites ) ) ) )
                     // TODO: <temp>
                       | view::parent
                       | view::child( view::all_of( alias_reqs | view::to_heading_set( kmap_ ) ) )
                     // TODO: </temp>
                       | view::parent
                       | view::parent
                       | view::child( "action" )
                       | view::to_node_set( kmap_ );

    for( auto const& action : actions )
    {
        KMAP_TRY( execute_body( action ) );
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap


