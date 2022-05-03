/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../db/cache.hpp"
#include "../../db/sm.hpp"
#include "../master.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::db;
using namespace kmap::db::sm;
namespace bsm = boost::sml;

SCENARIO( "unqiue lhs table sm is manipulated", "[db][cache][sm][ulhs_table]" )
{
    GIVEN( "an empty cache" )
    {
        auto cache = db::Cache{};
        auto [ decider, output ] = db::make_unique_cache_decider( cache );
        auto const tbl = HeadingTable{};
        auto const n1 = gen_uuid();
        auto const h1 = "h1";

        REQUIRE( decider->is( bsm::state< state::Start > ) );
        REQUIRE( !output->done );

        WHEN( "heading is pushed" )
        {
            auto const push = ev::Push{ .table = HeadingTable{}, .key = n1, .value = h1 };

            while( !output->done ) { decider->process_event( push ) ; }

            THEN( "create delta" )
            {
                REQUIRE( decider->is( bsm::state< state::CreateDelta > ) );
            }
        }
        WHEN( "erasing heading" )
        {
            auto const erase = ev::Erase{ .table = HeadingTable{}, .key = n1 };

            while( !output->done ) { decider->process_event( erase ) ; }

            THEN( "err" )
            {
                REQUIRE( decider->is( bsm::state< state::Error > ) );
            }
        }
    }
    GIVEN( "heading exists" )
    {
        auto cache = db::Cache{};
        auto [ decider, output ] = db::make_unique_cache_decider( cache );
        auto const tbl = HeadingTable{};
        auto const n1 = gen_uuid();
        auto const h1 = "h1";

        REQUIRE( succ( cache.push< HeadingTable >( n1, h1 ) ) );

        WHEN( "duplicate heading is pushed" )
        {
            auto const push = ev::Push{ .table = HeadingTable{}, .key = n1, .value = h1 };

            while( !output->done ) { decider->process_event( push ) ; }

            THEN( "do nothing" )
            {
                REQUIRE( decider->is( bsm::state< state::Nop > ) );
            }
        }
        WHEN( "replacing heading" )
        {
            auto const h2 = "h2";
            auto const push = ev::Push{ .table = HeadingTable{}, .key = n1, .value = h2 };

            while( !output->done ) { decider->process_event( push ) ; }

            THEN( "update delta" )
            {
                REQUIRE( decider->is( bsm::state< state::UpdateDelta > ) );
            }
        }
        WHEN( "erasing unpushed heading" )
        {
            auto const n2 = gen_uuid();
            auto const erase = ev::Erase{ .table = HeadingTable{}, .key = n2 };

            while( !output->done ) { decider->process_event( erase ) ; }

            THEN( "err" )
            {
                REQUIRE( decider->is( bsm::state< state::Error > ) );
            }
        }
        WHEN( "erasing pushed heading" )
        {
            auto const erase = ev::Erase{ .table = HeadingTable{}, .key = n1 };

            while( !output->done ) { decider->process_event( erase ) ; }

            THEN( "erase delta" )
            {
                REQUIRE( decider->is( bsm::state< state::EraseDelta > ) );
            }
        }
    }
}
