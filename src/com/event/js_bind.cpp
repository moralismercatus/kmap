/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/event/event.hpp>

#include <kmap/binding/js/result.hpp>

#include <emscripten.h>

namespace kmap::com::binding {

using namespace emscripten;

struct EventStore
{
    Kmap& kmap_;

    EventStore( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto fire_event( std::vector< std::string > const& requisites )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const estore = KTRY( kmap_.fetch_component< com::EventStore >() );

        return estore->fire_event( requisites | ranges::to< std::set >() );
    }
    auto fetch_payload()
        -> kmap::Result< com::EventStore::Payload >
    {
        KM_RESULT_PROLOG();

        auto rv = result::make_result< com::EventStore::Payload >();
        auto const estore = KTRY( kmap_.fetch_component< com::EventStore >() );

        if( auto const& pl = estore->fetch_payload()
          ; pl )
        {
            rv = pl.value();
        }

        return rv;
    }
    auto reset_transitions( std::vector< std::string > const& requisites )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

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
        .function( "fetch_payload", &kmap::com::binding::EventStore::fetch_payload )
        .function( "reset_transitions", &kmap::com::binding::EventStore::reset_transitions )
        ;
}

KMAP_BIND_RESULT( com::EventStore::Payload );

} // namespace binding