/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/database/cache.hpp"
#include "com/database/sm.hpp"
#include "test/master.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::com::db;
using namespace kmap::com::db::sm;
using namespace kmap::test;
namespace bsm = boost::sml;

SCENARIO( "unqiue lhs table sm is manipulated", "[db][cache][sm][ulhs_table]" )
{
    GIVEN( "an empty cache" )
    {
        auto cache = Cache{};
        auto decider = make_unique_cache_decider( cache );
        auto const tbl = HeadingTable{};
        auto const n1 = gen_uuid();
        auto const h1 = "h1";

        REQUIRE( decider.driver->is( bsm::state< state::Start > ) );
        REQUIRE( !decider.output->done );

        WHEN( "heading is pushed" )
        {
            auto const push = ev::Push{ .table = HeadingTable{}, .key = n1, .value = h1 };

            while( !decider.output->done ) { decider.driver->process_event( push ) ; }

            THEN( "create delta" )
            {
                REQUIRE( decider.driver->is( bsm::state< state::CreateDelta > ) );
            }
        }
        WHEN( "erasing heading" )
        {
            auto const erase = ev::Erase{ .table = HeadingTable{}, .key = n1 };

            while( !decider.output->done ) { decider.driver->process_event( erase ) ; }

            THEN( "err" )
            {
                REQUIRE( decider.driver->is( bsm::state< state::Error > ) );
            }
        }
    }
    GIVEN( "heading exists" )
    {
        auto cache = Cache{};
        auto decider = make_unique_cache_decider( cache );
        auto const tbl = HeadingTable{};
        auto const n1 = gen_uuid();
        auto const h1 = "h1";

        REQUIRE( succ( cache.push< HeadingTable >( n1, h1 ) ) );

        WHEN( "duplicate heading is pushed" )
        {
            auto const push = ev::Push{ .table = HeadingTable{}, .key = n1, .value = h1 };

            while( !decider.output->done ) { decider.driver->process_event( push ) ; }

            THEN( "do nothing" )
            {
                REQUIRE( decider.driver->is( bsm::state< state::Nop > ) );
            }
        }
        WHEN( "replacing heading" )
        {
            auto const h2 = "h2";
            auto const push = ev::Push{ .table = HeadingTable{}, .key = n1, .value = h2 };

            while( !decider.output->done ) { decider.driver->process_event( push ) ; }

            THEN( "update delta" )
            {
                REQUIRE( decider.driver->is( bsm::state< state::UpdateDelta > ) );
            }
        }
        WHEN( "erasing unpushed heading" )
        {
            auto const n2 = gen_uuid();
            auto const erase = ev::Erase{ .table = HeadingTable{}, .key = n2 };

            while( !decider.output->done ) { decider.driver->process_event( erase ) ; }

            THEN( "err" )
            {
                REQUIRE( decider.driver->is( bsm::state< state::Error > ) );
            }
        }
        WHEN( "erasing pushed heading" )
        {
            auto const erase = ev::Erase{ .table = HeadingTable{}, .key = n1 };

            while( !decider.output->done ) { decider.driver->process_event( erase ) ; }

            THEN( "erase delta" )
            {
                REQUIRE( decider.driver->is( bsm::state< state::ClearDelta > ) );
            }
        }
    }
}
