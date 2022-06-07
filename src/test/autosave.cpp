
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "chrono/timer.hpp"
#include "db.hpp"
#include "db/autosave.hpp"
#include "event/event.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "option/option.hpp"
#include "test/master.hpp"
#include "test/util.hpp"

#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/for_each.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "autosave" )
{
    KMAP_COMMAND_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();

    auto& db = kmap.database();
    auto& estore = kmap.event_store();
    auto& ostore = kmap.option_store();
    auto& timer = kmap.timer();
    auto& asave = kmap.autosave();

    REQUIRE( kmap.database().node_exists( kmap.root_node_id() ) );

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
        REQUIRE( succ( timer.install_default_timers() ) );
        REQUIRE( succ( asave.initialize() ) );
        REQUIRE( succ( ostore.apply_all() ) );
        REQUIRE( succ( estore.fire_event( { "subject.chrono.timer", "verb.intervaled", "object.chrono.minute" } ) ) );

        WHEN( "nothing flushed to disk" )
        {
            THEN( "no erase deltas in database" )
            {
                auto const& cache = db.cache();
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
            KMAP_INIT_DISK_DB_FIXTURE_SCOPED( db );

    fmt::print( "desc_count: {}\n", view::make( kmap.root_node_id() ) | view::desc | view::count( kmap ) );
            REQUIRE( succ( db.flush_delta_to_disk() ) );

            THEN( "root title exists in disk db" )
            {
                auto tt = titles::titles{};
                auto rows = db.execute( select( all_of( tt ) )
                                      . from( tt )
                                      . where( tt.title == "Root" ) );

                REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
            }

            WHEN( "root title is modified and autosave triggered" )
            {
                REQUIRE( succ( kmap.update_title( kmap.root_node_id(), "Rootie" ) ) );
                REQUIRE( succ( estore.fire_event( { "subject.chrono.timer", "verb.intervaled", "object.chrono.minute" } ) ) );
            }
        }
    }
}