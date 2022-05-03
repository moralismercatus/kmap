/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "common.hpp"
#include "error/master.hpp"

#include <catch2/catch_test_macros.hpp>

namespace utf = boost::unit_test;
using namespace kmap;

SCENARIO( "test kmap::Result" )
{
    GIVEN( "default Result< void >" )
    {
        auto r = KMAP_MAKE_RESULT( void );

        THEN( "explicit bool false" )
        {
            REQUIRE( fail( r ) );
        }
        THEN( "has_error" )
        {
            REQUIRE( r.has_error() );
        }
        THEN( "error succeeds" )
        {
            REQUIRE( r.error().ec == error_code::common::uncategorized );
        }
        THEN( "doesn't have value")
        {
            REQUIRE( !r.has_value() );
        }
        THEN( "value access throws" )
        {
            KMAP_DISABLE_EXCEPTION_LOG_SCOPED();

            REQUIRE_THROWS( r.value() );
        }
    }
    GIVEN( "default Result< void >" )
    {
        auto r = KMAP_MAKE_RESULT( void );

        WHEN( "set to success" )
        {
            r = outcome::success();

            THEN( "explicit bool true" )
            {
                REQUIRE( succ( r ) );
            }
            THEN( "doesn't have error" )
            {
                REQUIRE( !r.has_error() );
            }
            THEN( "has value" )
            {
                REQUIRE( r.has_value() );
            }
        }
    }
}