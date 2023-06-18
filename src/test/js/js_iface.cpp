/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"
#include "js_iface.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "testing eval operations", "[js][js_iface]" )
{
    GIVEN( "standard state" )
    {
        WHEN( "JS exception thrown" )
        {
            auto const res = js::eval_void( "if( 1 == 1 ){ throw 'this is an example of C++ catching a js exception'; }" );

            THEN( "error returned" )
            {
                REQUIRE( fail( res ) );
            }
            // TODO: It would be nice if it was communicated when js exception caught, but currently js::eval_failed propagated.
            // THEN( "ec is js_exception" )
            // {
            //     REQUIRE( res.error().ec == error_code::js::js_exception );
            // }
        }
    }
}