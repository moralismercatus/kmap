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
using namespace kmap::test;

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
                REQUIRE(( view::make( estore.event_root() )
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

SCENARIO( "objects fire in ascension", "[event]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto& estore = kmap.event_store();

    GIVEN( "nested object grammar" )
    {
        REQUIRE( succ( estore.install_subject( "network" ) ) );
        REQUIRE( succ( estore.install_verb( "depressed" ) ) );
        REQUIRE( succ( estore.install_object( "keyboard.key.ctrl" ) ) );

        GIVEN( "outlets for each object" )
        {
            REQUIRE( succ( estore.install_outlet( Leaf{ .heading="ctrl"
                                                      , .requisites={ "subject.network", "verb.depressed", "object.keyboard.key.ctrl" }
                                                      , .description={ "testing: objects firing in ascension" }
                                                      , .action={ R"%%%(kmap.create_child( kmap.root_node(), "ctrl" );)%%%" } } ) ) );
            REQUIRE( succ( estore.install_outlet( Leaf{ .heading = "key"
                                                      , .requisites={ "subject.network", "verb.depressed", "object.keyboard.key" }
                                                      , .description={ "testing: objects firing in ascension" }
                                                      , .action={ R"%%%(kmap.create_child( kmap.root_node(), "key" );)%%%" } } ) ) );
            REQUIRE( succ( estore.install_outlet( Leaf{ .heading = "keyboard"
                                                      , .requisites={ "subject.network", "verb.depressed", "object.keyboard" }
                                                      , .description={ "testing: objects firing in ascension" }
                                                      , .action={ R"%%%(kmap.create_child( kmap.root_node(), "keyboard" );)%%%" } } ) ) );

            WHEN( "fire nested event" )
            {
                REQUIRE( succ( estore.fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.ctrl" } ) ) );

                THEN( "result nodes created for each outlet" )
                {
                    REQUIRE( kmap.exists( "/ctrl" ) );
                    REQUIRE( kmap.exists( "/key" ) );
                    REQUIRE( kmap.exists( "/keyboard" ) );
                }
                THEN( "result nodes are in ascending order" )
                {
                    auto const ctrl     = REQUIRE_TRY( kmap.fetch_descendant( "/ctrl" ) );
                    auto const key      = REQUIRE_TRY( kmap.fetch_descendant( "/key" ) );
                    auto const keyboard = REQUIRE_TRY( kmap.fetch_descendant( "/keyboard" ) );
                    auto const ctrl_pos = REQUIRE_TRY( kmap.fetch_ordering_position( ctrl ) );
                    auto const key_pos  = REQUIRE_TRY( kmap.fetch_ordering_position( key ) );
                    auto const keyboard_pos = REQUIRE_TRY( kmap.fetch_ordering_position( keyboard ) );
                    REQUIRE( ctrl_pos < key_pos );
                    REQUIRE( key_pos < keyboard_pos );
                }
            }

            REQUIRE( kmap.exists( "/meta.event.outlet.ctrl" ) );
            REQUIRE( kmap.exists( "/meta.event.outlet.key" ) );
            REQUIRE( kmap.exists( "/meta.event.outlet.keyboard" ) );
            REQUIRE( succ( estore.uninstall_outlet( "ctrl" ) ) );
            REQUIRE( kmap.exists( "/meta.event.outlet.key" ) );
            REQUIRE( kmap.exists( "/meta.event.outlet.keyboard" ) );
            REQUIRE( succ( estore.uninstall_outlet( "key" ) ) );
            REQUIRE( kmap.exists( "/meta.event.outlet.keyboard" ) );
            // REQUIRE( succ( estore.uninstall_outlet( "keyboard" ) ) );
            // REQUIRE( !kmap.exists( "/meta.event.outlet.keyboard" ) );
        }
        
        REQUIRE( succ( estore.uninstall_subject( "network" ) ) );
        REQUIRE( succ( estore.uninstall_verb( "depressed" ) ) );
        // TODO: Very interesting. If I don't uninstall_outlet( "keyboard" ), prior to this, we have an error.
        //       This is _not_ true for "network" and "depressed". What's the difference? Nested alias, it would appear.
        //       Need to create a core alias test describing this behavior, and ensure it's failing, then fix the problem.
        // REQUIRE( succ( estore.uninstall_object( "keyboard" ) ) );
    }
}