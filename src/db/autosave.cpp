/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "db/autosave.hpp"

#include "common.hpp"
#include "db.hpp"
#include "event/event.hpp"
#include "kmap.hpp"
#include "option/option.hpp"

namespace kmap::db {

Autosave::Autosave( Kmap& kmap )
    : kmap_{ kmap }
{
}

auto Autosave::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& ostore = kmap_.option_store();

    {
        auto const script = 
R"%%%(
kmap.autosave().uninstall_event_outlet();
kmap.autosave().install_event_outlet( option_value ).throw_on_error();
)%%%";
        KMAP_TRY( ostore.install_option( "database.autosave.interval.unit"
                                       , "Autosave interval unit type from available timer types (second, minute, etc.)"
                                       , "\"minute\""
                                       , script ) );
    }
    {
        auto const script = 
R"%%%(
kmap.autosave().set_threshold( option_value );
)%%%";
        KMAP_TRY( ostore.install_option( "database.autosave.interval.threshold"
                                       , "number of interval events of type until autosave"
                                       , "1"
                                       , script ) );
    }

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
    auto& estore = kmap_.event_store();
    auto const action = 
R"%%%(
kmap.autosave().interval();
)%%%";
    auto const leaf = Leaf{ .heading = "autosave"
                          , .requisites = { "subject.chrono.timer", "verb.intervaled", fmt::format( "object.chrono.{}", unit ) }
                          , .description = "Saves deltas to disk every minute"
                          , .action = action };


    KMAP_TRY( estore.install_outlet( leaf ) );

    rv = outcome::success();

    return rv;
}

auto Autosave::uninstall_event_outlet()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& estore = kmap_.event_store();

    KMAP_TRY( estore.uninstall_outlet( "autosave" ) );

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
        auto& db = kmap_.database();

        if( db.has_file_on_disk() )
        {
            fmt::print( "autosave::interval() db has_file_on_disk\n" );
            KMAP_TRY( db.flush_delta_to_disk() );
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

} // namespace kmap::db