
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "com/canvas/canvas.hpp"
#include "js_iface.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "canvas" )
{
    // auto& km = Singleton::instance();
    // auto& canvas = kmap.canvas();

    GIVEN( "html canvas" )
    {
        REQUIRE( !js::element_exists( to_string( com::util_canvas_uuid ) ) ); 
        REQUIRE( succ( js::create_html_canvas( to_string( com::util_canvas_uuid ) ) ) );

        // canvas.
        // This would be a good opportunity to run through exactly how the canvas operates.
        WHEN( "" )
        {
            // canvas.root is created..
        }

        REQUIRE( succ( js::erase_child_element( to_string( com::util_canvas_uuid ) ) ) );
    }
}