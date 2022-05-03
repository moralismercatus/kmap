/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "event/event.hpp"
#include "js_iface.hpp"
#include "canvas.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;

SCENARIO( "event manipulation", "[event]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto& estore = kmap.event_store();

    GIVEN( "no event state" )
    {
        WHEN( "install action" )
        {
            REQUIRE( succ( estore.install_verb( "depressed" ) ) );

            THEN( "corresponding action node structrue exists" )
            {
                REQUIRE(( view::root( estore.event_root() )
                        | view::desc( "verb.depressed" )
                        | view::exists( kmap ) ));
            }

            REQUIRE( succ( kmap.root_view() 
                         | view::child( "meta" )
                         | view::erase_node( kmap ) ) );
        }
        WHEN( "install source" )
        {
            // REQUIRE( succ( estore.install_source( "keyboard.key_down"
            //                                     , "Key press down fired once until 'up' event."
            //                                     , R"%%%( kmap.network().install_keydown_handler().value_or_throw(); )%%%" ) ) );

            // THEN( "corresponding action node structrue exists" )
            // {
            //     REQUIRE(( kmap.make_view( estore.event_root() )
            //             | view::desc( ".source.network.key_down" )
            //             | view::exists ));
            //     REQUIRE(( kmap.make_view( estore.event_root() )
            //             | view::desc( ".source.network.key_down.description" )
            //             | view::exists ));
            //     REQUIRE(( kmap.make_view( estore.event_root() )
            //             | view::desc( ".source.network.key_down.installation" )
            //             | view::exists ));
            // }

            // auto const metanode = kmap.make_view() 
            //                     | view::child( "meta" )
            //                     | view::fetch_node; // TODO: view::erase_nodes
            // REQUIRE( succ( metanode ) );
            // REQUIRE( succ( kmap.erase_node( metanode.value() ) ) );
        }
    }
}
