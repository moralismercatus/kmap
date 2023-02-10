/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/database/query_cache.hpp"

#include "path/node_view2.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/iterator/operations.hpp>

#include <map>

namespace kmap::db {

// auto QueryCache::normalize( view2::Tether const& tether )
//     -> view2::Tether
// {
//     // TODO: Normalize... how to achieve this. The idea is to unfity queries into simplified form (e.g., direct_desc( "x.y" ) => child( "x" ) | child( "y" ) )
//     //       From here, the idea is to detect unique, but semantically equivalent queries. Whether this is actually an efficency improvement or not is TBD.
//     //       Example would be `X | child | parent | child` being equivalent to simply `X | child`.
//     return tether;
// }

auto QueryCache::clear()
    -> void
{
    // KMAP_LOG_LINE();
    map_.clear();
}

auto QueryCache::fetch( view2::Tether const& tether ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< UuidSet >();

    if( map_.contains( tether ) )
    {
        // fmt::print( "[QueryCache][fetched] {}\n", tether | act2::to_string );
        rv = map_.at( tether );
    }

    return rv;
}

auto QueryCache::push( view2::Tether const& tether
                     , UuidSet const& result )
    -> Result< void >
{
    // fmt::print( "[QueryCache][push] {}\n", tether | act2::to_string );
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    if( !map_.contains( tether ) )
    {
        map_.emplace( tether, result );

        rv = outcome::success();
    }

    return rv;
}

SCENARIO( "QueryCache push and fetch", "[node_view][query_cache]" )
{
    auto cache = QueryCache{};

    GIVEN( "empty cache" )
    {
        THEN( "map is empty" )
        {
            REQUIRE( ranges::distance( cache ) == 0 );
        }

        GIVEN( "cache.push( view, nodes )" )
        {
            auto const tv = anchor::abs_root
                          | view2::child
                          | view2::direct_desc( "echo.charlie" )
                          | view2::to_tether;
            auto const rns = UuidSet{ gen_uuid(), gen_uuid(), gen_uuid() };

            REQUIRE( test::fail( cache.fetch( tv ) ) );

            REQUIRE_RES( cache.push( tv, rns ) );
            REQUIRE( ranges::distance( cache ) == 1 );

            THEN( "cache.fetch( view )" )
            {
                auto const cached_nodes = REQUIRE_TRY( cache.fetch( tv ) );

                REQUIRE( cached_nodes == rns );
            }
            THEN( "!cache.fetch( abs_root | child | direct_desc( 'echo.victor' ) )" )
            {
                REQUIRE( test::fail( cache.fetch( anchor::abs_root
                                                | view2::child
                                                | view2::direct_desc( "echo.victor" )
                                                | view2::to_tether ) ) );
            }
        }
    }
    GIVEN( "cache.push( abs_root | direct_desc( 'meta.event.object' ), nodes )" )
    {
        auto const rns = UuidSet{ gen_uuid() };

        REQUIRE_RES( cache.push( anchor::abs_root | view2::direct_desc( "meta.event.object" ) | view2::to_tether, rns ) );
        REQUIRE( ranges::distance( cache ) == 1 );

        THEN( "cache.fetch( abs_root | direct_desc( 'meta.event.object' ) )" )
        {
            auto const cached_nodes = REQUIRE_TRY( cache.fetch( anchor::abs_root | view2::direct_desc( "meta.event.object" ) | view2::to_tether ) );

            REQUIRE( cached_nodes == rns );
        }
    }
}

auto QueryCache::begin() const
    -> TetherMap::const_iterator
{
    return map_.begin();
}

auto QueryCache::end() const
    -> TetherMap::const_iterator
{
    return map_.end();
}

} // kmap::db
