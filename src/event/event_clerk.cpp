/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "event/event_clerk.hpp"

#include "event/event.hpp"
#include "error/master.hpp"

#include <range/v3/view/reverse.hpp>

namespace kmap::event {

EventClerk::EventClerk( EventStore& event_store )
    : estore{ event_store }
{
}

EventClerk::~EventClerk()
{
    try
    {
        for( auto const& e : outlet_transitions | ranges::views::reverse ) { KTRYE( estore.uninstall_outlet( e ) ); }
        for( auto const& e : outlets | ranges::views::reverse ) { KTRYE( estore.uninstall_outlet( e ) ); }
        for( auto const& e : objects | ranges::views::reverse ) { KTRYE( estore.uninstall_object( e ) ); }
        for( auto const& e : verbs | ranges::views::reverse ) { KTRYE( estore.uninstall_verb( e ) ); }
        for( auto const& e : subjects | ranges::views::reverse ) { KTRYE( estore.uninstall_subject( e ) ); }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto EventClerk::install_subject( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const subject = KTRY( estore.install_subject( heading ) );

    subjects.emplace_back( subject );

    rv = subject;

    return rv;
}

auto EventClerk::install_verb( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const verb = KTRY( estore.install_verb( heading ) );

    verbs.emplace_back( verb );

    rv = verb;

    return rv;
}

auto EventClerk::install_object( Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const object = KTRY( estore.install_object( heading ) );

    objects.emplace_back( object );

    rv = object;

    return rv;
}

auto EventClerk::install_outlet( Leaf const& leaf ) 
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const outlet = KTRY( estore.install_outlet( leaf ) );

    outlets.emplace_back( outlet );

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_outlet( Branch const& branch ) 
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const outlet = KTRY( estore.install_outlet( branch ) );

    outlets.emplace_back( outlet );

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_outlet_transition( Uuid const& root
                                          , Transition const& transition )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const ot = KTRY( estore.install_outlet_transition( root, transition ) );

    outlet_transitions.emplace_back( ot );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::event
