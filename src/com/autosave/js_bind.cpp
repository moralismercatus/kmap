/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/autosave/autosave.hpp>

#include <emscripten.h>
#include <emscripten/bind.h>

namespace kmap::com::binding {

using namespace emscripten;

struct Autosave
{
    kmap::Kmap& km;

    Autosave( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto has_event_outlet()
        -> bool
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->has_event_outlet( { "subject.chrono.timer", "verb.intervaled" } );
    }

    auto has_event_outlet_unit( std::string const& unit )
        -> bool
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->has_event_outlet( { "subject.chrono.timer", "verb.intervaled", fmt::format( "object.chrono.{}", unit ) } );
    }
    
    auto interval()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->interval();
    }

    auto install_event_outlet( std::string const& unit )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->install_event_outlet( unit );
    }

    auto uninstall_event_outlet()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->uninstall_event_outlet();
    }

    auto set_threshold( uint32_t const threshold )
        -> void
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->set_threshold( threshold );
    }
};

auto autosave()
    -> binding::Autosave
{
    return binding::Autosave{ kmap::Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_autosave )
{
    function( "autosave", &kmap::com::binding::autosave );
    class_< kmap::com::binding::Autosave >( "Autosave" )
        .function( "has_event_outlet", &kmap::com::binding::Autosave::has_event_outlet )
        .function( "has_event_outlet_unit", &kmap::com::binding::Autosave::has_event_outlet_unit )
        .function( "interval", &kmap::com::binding::Autosave::interval )
        .function( "install_event_outlet", &kmap::com::binding::Autosave::install_event_outlet )
        .function( "uninstall_event_outlet", &kmap::com::binding::Autosave::uninstall_event_outlet )
        .function( "set_threshold", &kmap::com::binding::Autosave::set_threshold )
        ;
}

} // namespace kmap::com::binding

namespace
{
    namespace autosave_def
    {
        using namespace std::string_literals;

        REGISTER_COMPONENT
        (
            kmap::com::Autosave
        ,   std::set({ "database"s, "command.store"s, "event_store"s, "option_store"s, "tag_store"s })
        ,   "periodically pushes DB deltas to disk"
        );
    } // namespace autosave_def 
} // namespace anonymous