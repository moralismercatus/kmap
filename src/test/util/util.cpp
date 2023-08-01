/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "common.hpp"
#include "error/master.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "test kmap::Result" )
{
    GIVEN( "default Result< void >" )
    {
        auto r = result::make_result< void >();

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

            #if KMAP_EXCEPTIONS_ENABLED
            REQUIRE_THROWS( r.value() );
            #endif // KMAP_EXCEPTIONS_ENABLED
        }
    }
    GIVEN( "default Result< void >" )
    {
        auto r = result::make_result< void >();

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