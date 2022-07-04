/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "event/event_clerk.hpp"

#include "contract.hpp"
#include "error/master.hpp"
#include "event/event.hpp"
#include "kmap.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>

namespace kmap::event {

EventClerk::EventClerk( Kmap& km )
    : kmap{ km }
{
}

EventClerk::~EventClerk()
{
    try
    {
        auto& estore = kmap.event_store();
        auto const handle_result = []( auto const& res )
        {
            if( !res
             && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
            {
                KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
            }
        };

        for( auto const& e : outlet_transitions | ranges::views::reverse ) { handle_result( estore.uninstall_outlet( e ) ); }
        for( auto const& e : outlets | ranges::views::reverse ) { handle_result( estore.uninstall_outlet( e ) ); }
        for( auto const& e : objects | ranges::views::reverse ) { handle_result( estore.uninstall_object( e ) ); }
        for( auto const& e : verbs | ranges::views::reverse ) { handle_result( estore.uninstall_verb( e ) ); }
        for( auto const& e : subjects | ranges::views::reverse ) { handle_result( estore.uninstall_subject( e ) ); }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto EventClerk::fire_event( std::set< std::string > const& requisites )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap.event_store();
    auto const eroot = estore.event_root();

    KTRY( install_requisites( requisites ) );

    if( payload )
    {
        KTRY( estore.fire_event( requisites, payload.value() ) );
    }
    else
    {
        KTRY( estore.fire_event( requisites ) );
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::fire_event( std::set< std::string > const& requisites
                           , std::string const& event_payload )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    payload = event_payload;
    {
        KTRY( fire_event( requisites ) );
    }
    payload = std::nullopt; // TODO: Still need to null payload even if fire_event fails.

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_subject( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& estore = kmap.event_store();
    auto const subject = KTRY( estore.install_subject( heading ) );

    subjects.emplace_back( subject );

    rv = subject;

    return rv;
}

auto EventClerk::install_verb( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& estore = kmap.event_store();
    auto const verb = KTRY( estore.install_verb( heading ) );

    verbs.emplace_back( verb );

    rv = verb;

    return rv;
}

auto EventClerk::install_object( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& estore = kmap.event_store();
    auto const object = KTRY( estore.install_object( heading ) );

    objects.emplace_back( object );

    rv = object;

    return rv;
}

auto EventClerk::install_outlet( Leaf const& leaf ) 
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap.event_store();

    KTRY( install_requisites( leaf.requisites ) );

    {
        auto const outlet = KTRY( estore.install_outlet( leaf ) );

        outlets.emplace_back( outlet );
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_outlet( Branch const& branch ) 
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap.event_store();
    auto const outlet = KTRY( estore.install_outlet( branch ) );

    outlets.emplace_back( outlet );

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_outlet_transition( Uuid const& root
                                          , Transition const& transition )
    -> Result< void >
{
    KMAP_THROW_EXCEPTION_MSG( "TODO: Need to install_requisites() first" );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap.event_store();
    auto const ot = KTRY( estore.install_outlet_transition( root, transition ) );

    outlet_transitions.emplace_back( ot );

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_requisites( std::set< std::string > const& requisites )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap.event_store();
    auto const eroot = estore.event_root();

    for( auto const& req : requisites )
    {
        if( !( view::make( eroot )
             | view::direct_desc( req )
             | view::exists( kmap ) ) )
        {
            // TODO: Fix this...
            auto const ps = req
                          | ranges::views::split( '.' )
                          | ranges::views::drop( 1 )
                          | ranges::views::join( '.' )
                          | ranges::to< std::string >();

            if( req.starts_with( "subject" ) )
            {
                KTRY( install_subject( ps ) );
            }
            else if( req.starts_with( "verb" ) )
            {
                KTRY( install_verb( ps ) );
            }
            else if( req.starts_with( "object" ) )
            {
                KTRY( install_object( ps ) );
            }
            else
            {
                KMAP_THROW_EXCEPTION_MSG( "unexpected event requisite category" );
            }
        }
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap::event
