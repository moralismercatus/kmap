/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/autosave/autosave.hpp>

#include <com/database/db.hpp>
#include <com/event/event.hpp>
#include <com/network/network.hpp>
#include <com/option/option.hpp>
#include <common.hpp>
#include <kmap.hpp>
#include <util/result.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

#include <set>
#include <string>

namespace rvs = ranges::views;

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
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const ostore = KTRY( fetch_component< com::OptionStore >() );

    {
        auto const script = 
R"%%%(
const asave = kmap.autosave();

if( asave.has_event_outlet_unit( option_value ) )
{
    // Do nothing. Already exists.
}
else if( asave.has_event_outlet() )
{
    ktry( asave.uninstall_event_outlet() );
    ktry( asave.install_event_outlet( option_value ) );
}
else
{
    ktry( asave.install_event_outlet( option_value ) );
}
)%%%";
        KTRY( ostore->install_option( Option{ .heading = "database.autosave.interval.unit"
                                            , .descr = "Autosave interval unit type from available timer types (second, minute, etc.)"
                                            , .value = "\"minute\""
                                            , .action = script } ) );
    }
    {
        auto const script = 
R"%%%(
kmap.autosave().set_threshold( option_value );
)%%%";
        KTRY( ostore->install_option( Option{ .heading = "database.autosave.interval.threshold"
                                            , .descr = "number of interval events of type until autosave"
                                            , .value = "1"
                                            , .action = script } ) );
    }

    rv = outcome::success();

    return rv;
}

auto Autosave::load()
    -> Result< void >
{
    auto rv = result::make_result< void >();

    rv = outcome::success();

    return rv;
}

auto Autosave::has_event_outlet( std::set< std::string > const& requisites )
    -> bool
{
    KM_RESULT_PROLOG();

    auto rv = false;
    auto const estore = KTRYE( fetch_component< com::EventStore >() );
    auto const nw = KTRYE( fetch_component< com::Network >() );
    auto const mos = estore->fetch_matching_outlets( requisites );

    if( mos )
    {
        auto const mosv = mos.value();
        auto const matching_outlets = mosv
                                    | rvs::filter( [ & ]( auto const& e ){ return KTRYE( nw->fetch_heading( e ) ) == "autosave"; } )
                                    | ranges::to< UuidSet >();

        if( !matching_outlets.empty() )
        {
            // TODO: Unit test for this. Can occur when more than one outlet named "autosave" somewhere in the outlet tree.
            KMAP_ENSURE_EXCEPT( matching_outlets.size() == 1 );

            rv = true;
        }
    }

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "unit", unit );

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


// What I know so far:

// option_store->apply_all()
// -OptionStore::apply( "database.autosave.interval.unit" )
// --Autosave::uinstall_event_outlet
// --Autosave::install_event_outlet( "minute" )
// ---Kaboom!
// ---Here, actually:
//    There are a couple of questions I have about this:
//    1. Why is it failing?
//    2. Why is the failure being reported as a catastrophic error? throw on error() somehow not getting caught? Not even not getting caught, it's a fatal error; abort.
//    (2) is the more important of the two. It's only a matter of time before the next mysterious failure and bug hunt ensues....
    KTRY( estore->install_outlet( leaf ) );

    rv = outcome::success();

    return rv;
}

auto Autosave::uninstall_event_outlet()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( fetch_component< com::EventStore >() );

    KTRY( estore->uninstall_outlet( "autosave" ) );

    rv = outcome::success();

    return rv;
}


auto Autosave::interval()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    if( counter_ >= threshold_ )
    {
        auto const db = KTRY( fetch_component< com::Database >() );

        if( db->has_file_on_disk() )
        {
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

} // namespace kmap::com

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