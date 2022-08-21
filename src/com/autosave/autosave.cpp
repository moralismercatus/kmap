/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/autosave/autosave.hpp"

#include "com/database/db.hpp"
#include "com/event/event.hpp"
#include "com/option/option.hpp"
#include "common.hpp"
#include "emcc_bindings.hpp"
#include "kmap.hpp"

#include <set>
#include <string>

namespace kmap::com {

Autosave::Autosave( Kmap& kmap
                  , std::set< std::string > const& requisites
                  , std::string const& description )
    : Component{ kmap, requisites, description }
{
}

auto Autosave::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const ostore = KTRY( fetch_component< com::OptionStore >() );

    {
        auto const script = 
R"%%%(
kmap.autosave().uninstall_event_outlet();
kmap.autosave().install_event_outlet( option_value ).throw_on_error();
)%%%";
        KTRY( ostore->install_option( "database.autosave.interval.unit"
                                    , "Autosave interval unit type from available timer types (second, minute, etc.)"
                                    , "\"minute\""
                                    , script ) );
    }
    {
        auto const script = 
R"%%%(
kmap.autosave().set_threshold( option_value );
)%%%";
        KTRY( ostore->install_option( "database.autosave.interval.threshold"
                                    , "number of interval events of type until autosave"
                                    , "1"
                                    , script ) );
    }

    rv = outcome::success();

    return rv;
}

auto Autosave::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

// TODO: Question here: this depends on Timer - not that there'll be an error if Timer doesn't exist or that this is created before Timer,
//       but rather what happens if Timer never gets created? Autosave interval() will never fire. Not a big deal, but would kind of be nice
//       to know that autosave is effectively inoperable without Timer.
//       Correction: this does require "subject.chrono.timer", etc. to be pre-installed, meaning this does depend on Timer.
//       I suppose the question is whether the two should be functionally non-dependent, such that the outlet hopes the timer is active or will become
//       active. There's definitely an advantage in allowing an outlet to register for events before the source, so that there's not a hard and fast rule
//       about order of initialization. The downside is that it leaves open the window where the source (for some reason) may never actually fire, and the expected
//       behavior of the outlet never manifest - and, like autosave, an expected and mandatory behavior.
//       Perhaps install_outlet can take an additional boolean parameter in which the event aliases will be created if not found, and this way have the flexibility of
//       a requirement or an optional, i.e., if such a source fires the event, the outlet will be there ready for it.
auto Autosave::install_event_outlet( std::string const& unit )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( fetch_component< com::EventStore >() );
    auto const action = 
R"%%%(
kmap.autosave().interval();
)%%%";
    auto const leaf = Leaf{ .heading = "autosave"
                          , .requisites = { "subject.chrono.timer", "verb.intervaled", fmt::format( "object.chrono.{}", unit ) }
                          , .description = "Saves deltas to disk every minute"
                          , .action = action };


    KTRY( estore->install_outlet( leaf ) );

    rv = outcome::success();

    return rv;
}

auto Autosave::uninstall_event_outlet()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( fetch_component< com::EventStore >() );

    KTRY( estore->uninstall_outlet( "autosave" ) );

    rv = outcome::success();

    return rv;
}


auto Autosave::interval()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    fmt::print( "autosave::interval() entered\n" );

    if( counter_ >= threshold_ )
    {
        fmt::print( "autosave::interval() threshold met\n" );
        auto const db = KTRY( fetch_component< com::Database >() );

        if( db->has_file_on_disk() )
        {
            fmt::print( "autosave::interval() db has_file_on_disk\n" );
            KTRY( db->flush_delta_to_disk() );
        }

        counter_ = 0;
    }
    else
    {
        ++counter_;
    }

    rv = outcome::success();

    return rv;
}

auto Autosave::set_threshold( uint64_t const threshold )
    -> void
{
    threshold_ = threshold;
}

namespace binding {

using namespace emscripten;

struct Autosave
{
    kmap::Kmap& km;

    Autosave( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }
    
    auto interval()
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->interval();
    }

    auto install_event_outlet( std::string const& unit )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->install_event_outlet( unit );
    }

    auto uninstall_event_outlet()
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.fetch_component< kmap::com::Autosave >() )->uninstall_event_outlet();
    }

    auto set_threshold( uint32_t const threshold )
        -> void
    {
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
        .function( "interval", &kmap::com::binding::Autosave::interval )
        .function( "install_event_outlet", &kmap::com::binding::Autosave::install_event_outlet )
        .function( "uninstall_event_outlet", &kmap::com::binding::Autosave::uninstall_event_outlet )
        .function( "set_threshold", &kmap::com::binding::Autosave::set_threshold )
        ;
}

} // namespace binding

} // namespace kmap::com

namespace
{
    namespace autosave_def
    {
        using namespace std::string_literals;

        REGISTER_COMPONENT
        (
            kmap::com::Autosave
        ,   std::set({ "database"s, "command_store"s, "event_store"s, "option_store"s, "tag_store"s })
        ,   "periodically pushes DB deltas to disk"
        );
    } // namespace autosave_def 
} // namespace anonymous