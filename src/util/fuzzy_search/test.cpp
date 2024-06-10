/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <util/fuzzy_search/fuzzy_search.hpp>

namespace kmap {
} // namespace kmap

SCENARIO( "zadeh", "[fuzzy_search]" )
{
    using RSet = std::vector< std::string >;

    GIVEN( "set of famous book titles" )
    {
        auto candidates = RSet{ "Old Man's War"
                              , "The Lock Artist"
                              , "Right Ho Jeeves"
                              , "The Code of the Wooster"
                              , "Thank You Jeeves"
                              , "The DaVinci Code" };

        WHEN( "search: the code" )
        {
            // auto results = saf.filter_indices( "the code" );
            auto results = kmap::util::fuzzy_search( candidates, "the code", 10 );

            THEN( "match at least: titles with 'the' and 'code'" )
            {
                REQUIRE( results.size() >= 2 );
                REQUIRE( results[ 0 ] == candidates[ 3 ] );
                REQUIRE( results[ 1 ] == candidates[ 5 ] );
            }
        }
    }
}