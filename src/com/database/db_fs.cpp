/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "db_fs.hpp"

#include "com/cmd/cclerk.hpp"
#include "com/database/db.hpp"
#include "com/filesystem/filesystem.hpp"
#include "error/filesystem.hpp"
#include "kmap.hpp"
#include "test/util.hpp"
#include "util/signal_exception.hpp"
#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <vector>

namespace bfs = boost::filesystem;

namespace kmap::com {

DatabaseFilesystem::DatabaseFilesystem( Kmap& kmap
                                      , std::set< std::string > const& requisites
                                      , std::string const& description )
    : Component{ kmap, requisites, description }
    , cclerk_{ kmap }
{
}

auto DatabaseFilesystem::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( install_standard_commands() );

    rv = outcome::success();

    return rv;
}

auto DatabaseFilesystem::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // TODO: cclerk_.attach( "save.as" );?

    rv = outcome::success();

    return rv;
}

auto DatabaseFilesystem::install_standard_commands()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // save.as
    {
        auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
        auto const action_code =
R"%%%(```javascript
let rv = null;

const path = args.get( 0 );

if( !kmap.fs_path_exists( path ) )
{
    kmap.database().init_db_on_disk( path ).throw_on_error();
    kmap.database().flush_delta_to_disk().throw_on_error();

    rv = kmap.success( 'saved as: ' + path );
}
else
{
    rv = kmap.failure( "file already exists found" );
}

return rv;
```)%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "saves current state as new file on disk";
        auto const arguments = std::vector< Argument >{ Argument{ "new_file_path"
                                                                , "path where state file will be saved. Appends \".kmap\" if no extension given."
                                                                , "filesystem_path" } };
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "save.as"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        KTRY( cclerk_.install_command( command ) );
    }
    // save
    {
        auto const guard_code =
R"%%%(```javascript
if( kmap.database().has_file_on_disk() )
{
    return kmap.success( "file on disk found" );
}
else
{
    return kmap.failure( "failed to find file on disk" );
}
```)%%%";
        auto const action_code =
R"%%%(```javascript
let rv = null;

kmap.database().flush_delta_to_disk().throw_on_error();

rv = kmap.success( 'state saved to disk' );

return rv;
```)%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "saves current state to associated file on disk";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "file_is_on_disk", guard_code };
        auto const command = Command{ .path = "save"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        KTRY( cclerk_.install_command( command ) );
    }
    // load
    {
        auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
        // TODO: Verify reasoning about execution order.
        //       The theory is as follows: setTimeout() will not be triggered, no matter the time, until after the CLI command execution flow finishes because
        //       of the single-threaded event stack used by javascript. Be it 0ms or 100s. It must wait until the flow finishes before dispatching the timer.
        //       In this way, we should be able to trigger Kmap::load outside of any component (which will be destroyed by Kmap::load), avoiding corruption.
        auto const action_code =
R"%%%(```javascript
let rv = null;

const path = args.get( 0 );

if( kmap.fs_path_exists( path ) )
{
    // setTimeout( function(){ console.log( 'loading: ' + path ); kmap.load( path ).throw_on_error(); }, 1000 );
    kmap.throw_load_signal( path );

    rv = kmap.success( 'loading ' + path + '...' );
}
else
{
    rv = kmap.failure( "file not found: " + path );
}

return rv;
```)%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "loads state from disk";
        auto const arguments = std::vector< Argument >{ Argument{ "map_file_path"
                                                                , "path to map file"
                                                                , "filesystem_path" } };
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "load"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        KTRY( cclerk_.install_command( command ) );
    }

    rv = outcome::success();

    return rv;
}

SCENARIO( "save and load results in identical state", "[db][fs]")
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database.filesystem" );

    GIVEN( "minimal state" )
    {
        auto& km = Singleton::instance();
        auto const& db = REQUIRE_TRY( km.fetch_component< com::Database >() );
        auto const cache = db->cache();
        auto const disk_path = com::kmap_root_dir / ".db_fs_test.kmap";

        {
            // REQUIRE_RES( db->init_db_on_disk( disk_path ) );
            // REQUIRE_RES( db->flush_delta_to_disk() );
        }
        // db.tables

        if( exists( disk_path ) )
        {
            boost::filesystem::remove( disk_path );
        }
    }
}

auto DatabaseFilesystem::copy_state( FsPath const& dst )
    -> Result< void >
{
    using boost::system::error_code;

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( file_exists( dst ), kmap::error_code::filesystem::file_not_found );

    auto ec = error_code{};
    auto const db = KTRY( fetch_component< com::Database >() );

    bfs::copy_file( db->path()
                  , com::kmap_root_dir / dst
                  , ec );

    if( !ec )
    {
        rv = outcome::success();
    }

    return rv;
}

// [[noreturn]]?
auto DatabaseFilesystem::move_state( FsPath const& dst )
    -> Result< void >
{
    using boost::system::error_code;

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE_MSG( file_exists( dst ), kmap::error_code::filesystem::file_not_found, io::format( "unable to find file {}", dst.string() ) );

    auto ec = error_code{};
    auto const db = KTRY( fetch_component< com::Database >() );

    bfs::rename( db->path()
               , com::kmap_root_dir / dst
               , ec );

    KMAP_ENSURE_MSG( !ec, kmap::error_code::filesystem::file_rename_failure, io::format( "unable to rename file {}", dst.string() ) );

    // TODO: Shouldn't this be rather throw signal exception?
    throw SignalLoadException{ dst.string() };

    rv = outcome::success();

    return rv;
}

namespace {
namespace filesystem_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::DatabaseFilesystem
,   std::set({ "command_store"s, "database"s, "filesystem"s })
,   "database's filesystem support"
);

} // namespace filesystem_def 
}

namespace binding
{
    using namespace emscripten;

    EMSCRIPTEN_BINDINGS( kmap_database_filesystem )
    {
    }
} // namespace binding

} // namespace kmap::com
