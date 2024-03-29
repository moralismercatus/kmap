/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/database/cache.hpp"
#include "../master.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::com::db;
using namespace kmap::test;

SCENARIO( "cache is manipulated", "[cache]" )
{
    GIVEN( "umono table is empty" )
    {
        auto cache = Cache{};

        WHEN( "entry is pushed" )
        {
            auto const n1 = gen_uuid();

            REQUIRE( succ( cache.push< NodeTable >( n1 ) ) );

            THEN( "contains node" )
            {
                REQUIRE( cache.contains< NodeTable >( n1 ) );
                REQUIRE( cache.contains_delta< NodeTable >( n1 ) );
                REQUIRE( !cache.contains_cached< NodeTable >( n1 ) );
                REQUIRE( cache.fetch_value< NodeTable >( n1 ) );
            }
            THEN( "identical push NOPs" )
            {
                REQUIRE( succ( cache.push< NodeTable >( n1 ) ) );
            }
        }
    }
    GIVEN( "umono table has entry" )
    {
        auto cache = Cache{};
        auto const n1 = gen_uuid();

        REQUIRE( succ( cache.push< NodeTable >( n1 ) ) );

        REQUIRE( cache.contains< NodeTable >( n1 ) );
        REQUIRE( cache.contains_delta< NodeTable >( n1 ) );
        REQUIRE( !cache.contains_cached< NodeTable >( n1 ) );

        WHEN( "entry is deleted" )
        {
            REQUIRE( succ( cache.erase< NodeTable >( n1 ) ) );

            THEN( "table no longer contains entry" )
            {
                REQUIRE( !cache.contains< NodeTable >( n1 ) ); // No flush to disk, so clear from table.
                REQUIRE( !cache.contains_delta< NodeTable >( n1 ) ); // No flush to disk, so clear from table.
                REQUIRE( !cache.contains_cached< NodeTable >( n1 ) );
            }
        }
    }
    GIVEN( "ulhs table is empty" )
    {
        auto cache = com::db::Cache{};

        WHEN( "fetch heading" )
        {
            THEN( "fail to fetch" )
            {
                REQUIRE( fail( cache.fetch_value< HeadingTable >( gen_uuid() ) ) );
            }
        }
        WHEN( "push heading" )
        {
            auto const n1 = gen_uuid();
            auto const h1 = "h1";

            REQUIRE( succ( cache.push< HeadingTable >( n1, h1 ) ) );

            THEN( "fetch" )
            {
                REQUIRE( succ( cache.fetch_value< HeadingTable >( n1 ) ) );
            }
            AND_THEN( "fetch and compare" )
            {
                REQUIRE( cache.fetch_value< HeadingTable >( n1 ).value() == h1 );
            }
            
            REQUIRE( succ( cache.erase< HeadingTable >( n1 ) ) );
        }
        WHEN( "erase heading" )
        {
            auto const n1 = gen_uuid();

            THEN( "erase" )
            {
                REQUIRE( fail( cache.erase< HeadingTable >( n1 ) ) );
            }
        }
    }
    GIVEN( "ulhs table has delta heading entry" )
    {
        auto cache = com::db::Cache{};
        auto const n1 = gen_uuid();
        auto const h1 = "h1";

        REQUIRE( succ( cache.push< HeadingTable >( n1, h1 ) ) ); 

        WHEN( "fetch heading" )
        {
            REQUIRE( succ( cache.push< HeadingTable >( n1, h1 ) ) );

            THEN( "fetch" )
            {
                REQUIRE( succ( cache.fetch_value< HeadingTable >( n1 ) ) );
            }
            AND_THEN( "fetch and compare" )
            {
                REQUIRE( succ( cache.fetch_value< HeadingTable >( n1 ).value() == h1 ) );
            }
            AND_THEN( "fail to fetch other" )
            {
                REQUIRE( fail( cache.fetch_value< HeadingTable >( gen_uuid() ) ) );
            }

            REQUIRE( succ( cache.erase< HeadingTable >( n1 ) ) );
        }
        WHEN( "push duplicate" )
        {
            REQUIRE( succ( cache.push< HeadingTable >( n1, h1 ) ) ); // Not an error. Pushing duplicate is a NOP.

            THEN( "fetch" )
            {
                REQUIRE( succ( cache.fetch_value< HeadingTable >( n1 ) ) );
            }
        }
    }
    GIVEN( "ulr table is empty" )
    {
        auto cache = com::db::Cache{};

        WHEN( "entry is created" )
        {
            auto const n1 = gen_uuid();
            auto const n2 = gen_uuid();

            // TODO: contains (and variants) should allow Left, Right arguemnts, rather than forcing unique_key_type{ left, right }
            REQUIRE( !cache.contains< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            REQUIRE( !cache.contains_delta< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            REQUIRE( !cache.contains_cached< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            REQUIRE( !cache.contains_erased_delta< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );

            REQUIRE( succ( cache.push< AliasTable >( Src{ n2 }, Dst{ n1 } ) ) );

            REQUIRE( cache.contains< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            REQUIRE( cache.contains_delta< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            REQUIRE( !cache.contains_cached< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            REQUIRE( !cache.contains_erased_delta< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );

            THEN( "is erased" )
            {
                REQUIRE( succ( cache.erase< AliasTable >( Src{ n2 }, Dst{ n1 } ) ) );

                REQUIRE( !cache.contains< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
                REQUIRE( !cache.contains_delta< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
                REQUIRE( !cache.contains_cached< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
                REQUIRE( !cache.contains_erased_delta< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            }
        }
    }
    GIVEN( "ulr table has delta entry" )
    {
        auto cache = Cache{};
        auto const n1 = gen_uuid();
        auto const n2 = gen_uuid();

        REQUIRE( succ( cache.push< AliasTable >( Src{ n2 }, Dst{ n1 } ) ) );

        THEN( "alias src and dst exists" )
        {
            REQUIRE( cache.contains< AliasTable >( AliasTable::unique_key_type{ Src{ n2 }, Dst{ n1 } } ) );
            REQUIRE( cache.contains< AliasTable >( Src{ n2 } ) );
            REQUIRE( cache.contains< AliasTable >( Dst{ n1 } ) );
            REQUIRE( !cache.contains< AliasTable >( Src{ n1 } ) );
            REQUIRE( !cache.contains< AliasTable >( Dst{ n2 } ) );
        }

        REQUIRE( succ( cache.erase< AliasTable >( Src{ n2 }, Dst{ n1 } ) ) );
    }
}
