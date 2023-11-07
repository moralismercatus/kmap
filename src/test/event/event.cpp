/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"
#include "com/canvas/canvas.hpp"
#include "com/event/event.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "event manipulation", "[event]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "event_store" );

    auto& kmap = Singleton::instance();
    auto const estore = REQUIRE_TRY( kmap.fetch_component< com::EventStore >() );

    GIVEN( "no event state" )
    {
        WHEN( "install action" )
        {
            REQUIRE( succ( estore->install_verb( "depressed" ) ) );

            THEN( "corresponding action node structrue exists" )
            {
                REQUIRE(( view2::event::verb_root
                        | view2::direct_desc( "depressed" )
                        | act2::exists( kmap ) ));
            }

            REQUIRE( succ( anchor::abs_root
                         | view2::child( "meta" )
                         | act2::erase_node( kmap ) ) );
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
        REQUIRE( succ( estore->install_object( "keyboard.key.control" ) ) );

        GIVEN( "outlets for each object" )
        {
            REQUIRE( succ( estore->install_outlet( com::Leaf{ .heading="control"
                                                            , .requisites={ "subject.network", "verb.depressed", "object.keyboard.key.control" }
                                                            , .description={ "testing: objects firing in ascension" }
                                                            , .action={ R"%%%(kmap.create_child( kmap.root_node(), "control" );)%%%" } } ) ) );
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
                REQUIRE( succ( estore->fire_event( { "subject.network", "verb.depressed", "object.keyboard.key.control" } ) ) );

                THEN( "result nodes created for each outlet" )
                {
                    REQUIRE( nw->exists( "/control" ) );
                    REQUIRE( nw->exists( "/key" ) );
                    REQUIRE( nw->exists( "/keyboard" ) );
                }
                THEN( "result nodes are in ascending order" )
                {
                    auto const control     = REQUIRE_TRY( anchor::abs_root | view2::child( "control" ) | act2::fetch_node( kmap ) );
                    auto const key      = REQUIRE_TRY( anchor::abs_root | view2::child( "key" ) | act2::fetch_node( kmap ) );
                    auto const keyboard = REQUIRE_TRY( anchor::abs_root | view2::child( "keyboard" ) | act2::fetch_node( kmap ) );
                    auto const control_pos = REQUIRE_TRY( nw->fetch_ordering_position( control ) );
                    auto const key_pos  = REQUIRE_TRY( nw->fetch_ordering_position( key ) );
                    auto const keyboard_pos = REQUIRE_TRY( nw->fetch_ordering_position( keyboard ) );
                    REQUIRE( control_pos < key_pos );
                    REQUIRE( key_pos < keyboard_pos );
                }
            }

            REQUIRE( nw->exists( "/meta.event.outlet.control" ) );
            REQUIRE( nw->exists( "/meta.event.outlet.key" ) );
            REQUIRE( nw->exists( "/meta.event.outlet.keyboard" ) );
            REQUIRE( succ( estore->uninstall_outlet( "control" ) ) );
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