/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/cli/cli.hpp>
#include <com/filesystem/filesystem.hpp>
#include <test/util.hpp>

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>

namespace fs = boost::filesystem;

namespace kmap {

SCENARIO( "task activates and deactivates in presence of db on disk", "[cli][db][fs][tag][task][save]")
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database", "database.filesystem", "cli", "database.filesystem.command", "network", "tag_store", "task_store" );

    auto& km = Singleton::instance();
    auto const disk_path = ".db_fs_test.kmap";
    auto const abs_disk_path = com::kmap_root_dir / disk_path;
    auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );

    if( fs::exists( abs_disk_path ) )
    {
        fs::remove( abs_disk_path );
    }

    GIVEN( ":save.as <file>" )
    {
        REQUIRE_TRY( cli->parse_raw( fmt::format( ":save.as {}", disk_path ) ) );

        GIVEN( ":create.task t1" )
        {
            REQUIRE_TRY( cli->parse_raw( ":create.task t1" ) );
            // +t1.$.tag.inactive

            GIVEN( ":save" )
            {
                REQUIRE_TRY( cli->parse_raw( ":save" ) );

                GIVEN( ":activate.task <t1>" )
                {
                    REQUIRE_TRY( cli->parse_raw( ":activate.task" ) );
                    // -t1.$.tag.inactive
                    // +t1.$.tag.active

                    GIVEN( ":deactivate.task <t1>" )
                    {
                        // Note: Error appeared here only when file on disk.
                        REQUIRE_TRY( cli->parse_raw( ":deactivate.task" ) );
                        // -t1.$.tag.active
                        // +t1.$.tag.inactive
                    }
                }
            }
        }
    }

    if( fs::exists( abs_disk_path ) )
    {
        fs::remove( abs_disk_path );
    }
}

} // namespace kmap