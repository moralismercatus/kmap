/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/database/filesystem/command.hpp>

#include <com/database/db.hpp>
#include <com/database/filesystem/db_fs.hpp>

#include <vector>

namespace bfs = boost::filesystem;
namespace rvs = ranges::views;

namespace kmap::com::db {

Command::Command( Kmap& kmap
                , std::set< std::string > const& requisites
                , std::string const& description )
    : Component{ kmap, requisites, description }
    , cclerk_{ kmap }
{
    KM_RESULT_PROLOG();

    KTRYE( register_standard_commands() );
}

auto Command::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto Command::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.check_registered() );
    // TODO: cclerk_.attach( "save.as" );?

    rv = outcome::success();

    return rv;
}

auto Command::register_standard_commands()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    // save.as
    {
        auto const action_code =
        R"%%%(
            const path = args.get( 0 );

            if( !kmap.fs_path_exists( path ) )
            {
                ktry( kmap.database().init_db_on_disk( path ) );
                ktry( kmap.database().flush_cache_to_disk() );
            }
            else
            {
                return kmap.failure( 'file already exists found' );
            }
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "saves current state as new file on disk";
        auto const arguments = std::vector< Argument >{ Argument{ "new_file_path"
                                                                , "path where state file will be saved. Appends \".kmap\" if no extension given."
                                                                , "filesystem_path" } };
        auto const command = com::Command{ .path = "save.as"
                                         , .description = description
                                         , .arguments = arguments
                                         , .guard = "unconditional"
                                         , .action = action_code };

        KTRY( cclerk_.register_command( command ) );
    }
    // save
    {
        auto const action_code =
        R"%%%(
            ktry( kmap.database().flush_delta_to_disk() );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "saves current state to associated file on disk";
        auto const arguments = std::vector< Argument >{};
        auto const command = com::Command{ .path = "save"
                                         , .description = description
                                         , .arguments = arguments
                                         , .guard = "unconditional" // TODO: has_file_on_disk()
                                         , .action = action_code };

        KTRY( cclerk_.register_command( command ) );
    }
    // load
    {
        // TODO: Verify reasoning about execution order.
        //       The theory is as follows: setTimeout() will not be triggered, no matter the time, until after the CLI command execution flow finishes because
        //       of the single-threaded event stack used by javascript. Be it 0ms or 100s. It must wait until the flow finishes before dispatching the timer.
        //       In this way, we should be able to trigger Kmap::load outside of any component (which will be destroyed by Kmap::load), avoiding corruption.
        auto const action_code =
        R"%%%(
            const path = args.get( 0 );

            if( kmap.fs_path_exists( path ) )
            {
                kmap.throw_load_signal( path );
            }
            else
            {
                return kmap.failure( "file not found: " + path );
            }
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "loads state from disk";
        auto const arguments = std::vector< Argument >{ Argument{ "map_file_path"
                                                                , "path to map file"
                                                                , "filesystem_path" } };
        auto const command = com::Command{ .path = "load"
                                         , .description = description
                                         , .arguments = arguments
                                         , .guard = "unconditional"
                                         , .action = action_code };

        KTRY( cclerk_.register_command( command ) );
    }

    rv = outcome::success();

    return rv;
}

namespace {
namespace database_filesystem_command_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::db::Command
,   std::set({ "database.filesystem"s, "command.store"s, "command.standard_items"s })
,   "database.filesystem's command support"
);

} // namespace database_filesystem_command_def  
}

} // namespace kmap::com::db
