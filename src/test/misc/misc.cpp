/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/common.hpp>
#include <com/cli/cli.hpp>
#include <com/database/db.hpp>
#include <com/database/db_fs.hpp>
#include <com/event/event.hpp>
#include <com/filesystem/filesystem.hpp>
#include <component.hpp>
#include <test/util.hpp>

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>

namespace kmap {

SCENARIO( "Erase canvas node within save/load", "[canvas][db][fs][save][load]" )
{
    auto& km = Singleton::instance();
    auto const db = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Database >() ); };

    GIVEN( "all components initialized" )
    {
        auto const all_components = REQUIRE_TRY( fetch_listed_components() );

        KMAP_COMPONENT_FIXTURE_SCOPED( all_components );

        auto const disk_path = ".db_fs_test.kmap";
        auto const abs_disk_path = com::kmap_root_dir / disk_path;

        GIVEN( "erase canvas root node" )
        {
            REQUIRE(( view2::canvas::canvas_root | act2::exists( km ) ));

            REQUIRE_TRY(( view2::canvas::canvas_root
                        | act2::erase_node( km ) ));

            GIVEN( "init_db_on_disk" )
            {
                REQUIRE_TRY( db()->init_db_on_disk( abs_disk_path ) );

                GIVEN( "flush_delta_to_disk" )
                {
                    REQUIRE_RES( db()->flush_delta_to_disk() );

                    GIVEN( "load" )
                    {
                        KMAP_LOG_LINE();
                        REQUIRE_RES( km.load( disk_path, all_components ) );
                        KMAP_LOG_LINE();

                        THEN( "enter CLI command bar" )
                        {
                            auto const estore = REQUIRE_TRY( km.fetch_component< com::EventStore >() );

                            // Trigger command bar focus.
                            REQUIRE_TRY( estore->fire_event( { "subject.window", "verb.depressed", "object.keyboard.key.shift" } ) );
                            REQUIRE_TRY( estore->fire_event( { "subject.window", "verb.depressed", "object.keyboard.key.colon" } ) );

                            THEN( "execute command" )
                            {
                                auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );

                                REQUIRE_TRY( cli->on_key_down( 13 // enter
                                                             , false
                                                             , false
                                                             , ":@root" ) );
                            }
                        }
                    }
                }
            }
        }

        if( exists( abs_disk_path ) ) { boost::filesystem::remove( abs_disk_path ); }
    }
}

} // namespace kmap
