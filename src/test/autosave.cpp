
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/autosave/autosave.hpp"

#include "com/chrono/timer.hpp"
#include "com/database/db.hpp"
#include "com/database/table_decl.hpp"
#include "com/event/event.hpp"
#include "com/network/network.hpp"
#include "com/option/option.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "test/master.hpp"
#include "test/util.hpp"

#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/for_each.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sqlpp11/sqlite3/connection.h>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "autosave", "[com][autosave]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "autosave", "timer", "visnetwork", "network" ); // TODO: drop "visnetwork" component after update_title changes use of event instead of direct call.

    auto& kmap = Singleton::instance();

    auto const db = REQUIRE_TRY( kmap.fetch_component< com::Database >() );
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );
    auto const ostore = REQUIRE_TRY( kmap.fetch_component< com::OptionStore >() );
    auto const timer = REQUIRE_TRY( kmap.fetch_component< com::Timer >() );
    auto const asave = REQUIRE_TRY( kmap.fetch_component< com::Autosave >() );
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );

    REQUIRE( db->node_exists( kmap.root_node_id() ) );

    // Steps:
    // As always, bare minimum to test:
    // 1. Create minimal root structure
    // 1. Create event structures.
    // 1. Create autosave structures.
    // 1. Save to disk.
    // 1. Check title "Root" on disk.
    // 1. Update "Root" to "Rootie".
    // 1. Force autosave by firing events.
    // 1. Check title "Rootie" on disk.
    GIVEN( "autotimer infrastructure initialized" )
    {
        REQUIRE( succ( timer->install_default_timers() ) ); // TODO: Is this even used, if fire_event is called manually?
        REQUIRE( succ( asave->initialize() ) );
        REQUIRE( succ( ostore->apply_all() ) );
        REQUIRE( succ( estore->fire_event( { "subject.chrono.timer", "verb.intervaled", "object.chrono.minute" } ) ) );

        WHEN( "nothing flushed to disk" )
        {
            THEN( "no erase deltas in database" )
            {
                auto const& cache = db->cache();
                auto const proc_table = [ & ]( auto const& table )
                {
                    for( auto const& t_item : table )
                    {
                        REQUIRE( 0 == ranges::count_if( t_item.delta_items, []( auto const& e ){ return e.action == db::DeltaType::erased; } ) );
                    }
                };

                boost::hana::for_each( cache.tables(), proc_table );
            }
        }

        WHEN( "save to disk" )
        {
            KMAP_INIT_DISK_DB_FIXTURE_SCOPED( *db );

            REQUIRE( succ( db->flush_delta_to_disk() ) );

            THEN( "root title exists in disk db" )
            {
                auto tt = titles::titles{};
                auto rows = db->execute( select( all_of( tt ) )
                                       . from( tt )
                                       . where( tt.title == "Root" ) );

                REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
            }

            WHEN( "root title is modified and autosave triggered" )
            {
                REQUIRE( succ( nw->update_title( kmap.root_node_id(), "Rootie" ) ) );
                REQUIRE( succ( estore->fire_event( { "subject.chrono.timer", "verb.intervaled", "object.chrono.minute" } ) ) );
            }
        }
    }
}