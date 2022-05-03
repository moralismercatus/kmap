/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"
#include "js_iface.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;

SCENARIO( "testing eval operations" )
{
    GIVEN( "standard state" )
    {
        WHEN( "JS exception thrown" )
        {
            auto const res = js::eval_void( "throw 'this is an example of C++ catching a js exception';" );

            THEN( "error returned" )
            {
                REQUIRE( fail( res ) );
            }
            THEN( "ec is js_exception" )
            {
                REQUIRE( res.error().ec == error_code::js::js_exception );
            }
        }
    }
}