/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "com/canvas/canvas.hpp"
#include "com/event/event.hpp"
#include "js_iface.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "event manipulation", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );
    auto const eroot = REQUIRE_TRY( estore->event_root() );

    GIVEN( "no event state" )
    {
        WHEN( "install action" )
        {
            REQUIRE( succ( estore->install_verb( "depressed" ) ) );

            THEN( "corresponding action node structrue exists" )
            {
                REQUIRE(( view::make( eroot )
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
            // REQUIRE( succ( nw->erase_node( metanode.value() ) ) );
        }
    }
}

SCENARIO( "objects fire in ascension", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );

    GIVEN( "nested object grammar" )
    {
        REQUIRE( succ( estore->install_subject( "network" ) ) );
        REQUIRE( succ( estore->install_verb( "depressed" ) ) );
        REQUIRE( succ( estore->install_object( "keyboard.key.ctrl" ) ) );

        GIVEN( "outlets for each object" )
        {
            REQUIRE( succ( estore->install_outlet( com::Leaf{ .heading="ctrl"
                                                            , .requisites={ "subject.network", "verb.depressed", "object.keyboard.key.ctrl" }
                                                            , .description={ "testing: objects firing in ascension" }
                                                            , .action={ R"%%%(kmap.create_child( kmap.root_node(), "ctrl" );)%%%" } } ) ) );
            REQUIRE( succ( estore->install_outlet( com::Leaf{ .heading = "key"
                                                            , .requisites={ "subject.network", "verb.depressed", "object.keyboard.key" }
                                                            , .description={ "testing: objects firing in ascension" }
                                                            , .action={ R"%%%(kmap.create_child( kmap.root_node(), "key" );)%%%" } } ) ) );
            REQUIRE( succ( estore->install_outlet( com::Leaf{ .heading = "keyboard"
                                                            , .requisites={ "subject.network", "verb.depressed", "object.keyboard" }
                                                            , .description={ "testing: objects firing in ascension" }
                                                            , .action={ R"%%%(kmap.create_child( kmap.root_node(), "keyboard" );)%%%" } } ) ) );

            WHEN( "fire nested event" )
            {
                REQUIRE( succ( estore->fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.ctrl" } ) ) );

                THEN( "result nodes created for each outlet" )
                {
                    REQUIRE( nw->exists( "/ctrl" ) );
                    REQUIRE( nw->exists( "/key" ) );
                    REQUIRE( nw->exists( "/keyboard" ) );
                }
                THEN( "result nodes are in ascending order" )
                {
                    auto const ctrl     = REQUIRE_TRY( kmap.root_view() | view::child( "ctrl" ) | view::fetch_node( kmap ) );
                    auto const key      = REQUIRE_TRY( kmap.root_view() | view::child( "key" ) | view::fetch_node( kmap ) );
                    auto const keyboard = REQUIRE_TRY( kmap.root_view() | view::child( "keyboard" ) | view::fetch_node( kmap ) );
                    auto const ctrl_pos = REQUIRE_TRY( nw->fetch_ordering_position( ctrl ) );
                    auto const key_pos  = REQUIRE_TRY( nw->fetch_ordering_position( key ) );
                    auto const keyboard_pos = REQUIRE_TRY( nw->fetch_ordering_position( keyboard ) );
                    REQUIRE( ctrl_pos < key_pos );
                    REQUIRE( key_pos < keyboard_pos );
                }
            }

            REQUIRE( nw->exists( "/meta.event.outlet.ctrl" ) );
            REQUIRE( nw->exists( "/meta.event.outlet.key" ) );
            REQUIRE( nw->exists( "/meta.event.outlet.keyboard" ) );
            REQUIRE( succ( estore->uninstall_outlet( "ctrl" ) ) );
            REQUIRE( nw->exists( "/meta.event.outlet.key" ) );
            REQUIRE( nw->exists( "/meta.event.outlet.keyboard" ) );
            REQUIRE( succ( estore->uninstall_outlet( "key" ) ) );
            REQUIRE( nw->exists( "/meta.event.outlet.keyboard" ) );
            // REQUIRE( succ( estore->uninstall_outlet( "keyboard" ) ) );
            // REQUIRE( !nw->exists( "/meta.event.outlet.keyboard" ) );
        }
        
        REQUIRE( succ( estore->uninstall_subject( "network" ) ) );
        REQUIRE( succ( estore->uninstall_verb( "depressed" ) ) );
        // TODO: Very interesting. If I don't uninstall_outlet( "keyboard" ), prior to this, we have an error.
        //       This is _not_ true for "network" and "depressed". What's the difference? Nested alias, it would appear.
        //       Need to create a core alias test describing this behavior, and ensure it's failing, then fix the problem.
        // REQUIRE( succ( estore->uninstall_object( "keyboard" ) ) );
    }
}