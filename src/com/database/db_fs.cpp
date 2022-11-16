/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "db_fs.hpp"

#include "com/cmd/cclerk.hpp"
#include "com/database/db.hpp"
#include "com/filesystem/filesystem.hpp"
#include "db.hpp"
#include "error/filesystem.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "table_decl.hpp"
#include "test/util.hpp"
#include "util/signal_exception.hpp"
#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/sqlite3/connection.h>

#include <vector>

namespace bfs = boost::filesystem;
namespace rvs = ranges::views;

namespace kmap::com {

DatabaseFilesystem::DatabaseFilesystem( Kmap& kmap
                                      , std::set< std::string > const& requisites
                                      , std::string const& description )
    : Component{ kmap, requisites, description }
    , cclerk_{ kmap }
{
    register_standard_commands();
}

auto DatabaseFilesystem::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto DatabaseFilesystem::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.check_registered() );
    // TODO: cclerk_.attach( "save.as" );?

    rv = outcome::success();

    return rv;
}

// TODO: Should go under db.fs.cmd component, to alleviate dependence on command_store for db.fs (perhaps).
//       And, to make standard practice out of isolating commands for a component, so it is not doing more than one job.
auto DatabaseFilesystem::register_standard_commands()
    -> void
{
    // save.as
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        auto const action_code =
        R"%%%(
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
        )%%%";

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

        cclerk_.register_command( command );
    }
    // save
    {
        auto const guard_code =
        R"%%%(
            if( kmap.database().has_file_on_disk() )
            {
                return kmap.success( "file on disk found" );
            }
            else
            {
                return kmap.failure( "failed to find file on disk" );
            }
        )%%%";
        auto const action_code =
        R"%%%(
            let rv = null;

            kmap.database().flush_delta_to_disk().throw_on_error();

            rv = kmap.success( 'state saved to disk' );

            return rv;
        )%%%";

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

        cclerk_.register_command( command );
    }
    // load
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'success' );
        )%%%";
        // TODO: Verify reasoning about execution order.
        //       The theory is as follows: setTimeout() will not be triggered, no matter the time, until after the CLI command execution flow finishes because
        //       of the single-threaded event stack used by javascript. Be it 0ms or 100s. It must wait until the flow finishes before dispatching the timer.
        //       In this way, we should be able to trigger Kmap::load outside of any component (which will be destroyed by Kmap::load), avoiding corruption.
        auto const action_code =
        R"%%%(
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
        )%%%";

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

        cclerk_.register_command( command );
    }
}

SCENARIO( "saved data mirrors runtime data", "[db][fs]")
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database.filesystem", "network" );

    using namespace sqlpp;

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const db = REQUIRE_TRY( km.fetch_component< com::Database >() );
    auto const disk_path = ".db_fs_test.kmap";
    auto const abs_disk_path = com::kmap_root_dir / disk_path;
    auto const root = nw->root_node();
    auto nt = nodes::nodes{};
    auto ht = headings::headings{};
    auto ct = children::children{};
    auto const exec_select = [ & ]( auto const& table
                                  , auto const& key )
    {
        return db->execute( select( all_of( table ) )
                          . from( table )
                          . where( key ) );
    };

    GIVEN( "initit db on disk" )
    {
        if( exists( abs_disk_path ) )
        {
            boost::filesystem::remove( abs_disk_path );
        }

        REQUIRE_RES( db->init_db_on_disk( abs_disk_path ) );

        GIVEN( "root node flush" )
        {
            REQUIRE_RES( db->flush_delta_to_disk() );

            THEN( "root on disk" )
            {
                // nt
                {
                    auto rows = exec_select( nt, nt.uuid == to_string( root ) );
                    REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                }
                // ht
                {
                    auto rows = exec_select( ht, ht.uuid == to_string( root ) );
                    REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                }
                {
                    auto rows = exec_select( ht, ht.uuid == to_string( root ) );
                    REQUIRE( rows.begin()->heading == "root" );
                }
            }

            GIVEN( "child node flush" )
            {
                auto const c1h = "1";
                auto const c1 = REQUIRE_TRY( nw->create_child( root, c1h ) );
                REQUIRE_RES( db->flush_delta_to_disk() );

                THEN( "child on disk" )
                {
                    // nt
                    {
                        auto rows = exec_select( nt, nt.uuid == to_string( c1 ) );
                        REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                    }
                    // ht
                    {
                        auto rows = exec_select( ht, ht.uuid == to_string( c1 ) );
                        REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                    }
                    {
                        auto rows = exec_select( ht, ht.uuid == to_string( c1 ) );
                        REQUIRE( rows.begin()->heading == c1h );
                    }
                    // ct
                    {
                        auto rows = exec_select( ct, ct.child_uuid == to_string( c1 ) );
                        REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                    }
                }
                
                GIVEN( "child erase flush" )
                {
                    REQUIRE_RES( nw->erase_node( c1 ) );
                    REQUIRE_RES( db->flush_delta_to_disk() );

                    THEN( "child NOT on disk" )
                    {
                        // nt
                        {
                            auto rows = exec_select( nt, nt.uuid == to_string( c1 ) );
                            REQUIRE( std::distance( rows.begin(), rows.end() ) == 0 );
                        }
                        // ht
                        {
                            auto rows = exec_select( ht, ht.uuid == to_string( c1 ) );
                            REQUIRE( std::distance( rows.begin(), rows.end() ) == 0 );
                        }
                        // ct
                        {
                            auto rows = exec_select( ct, ct.child_uuid == to_string( c1 ) );
                            REQUIRE( std::distance( rows.begin(), rows.end() ) == 0 );
                        }
                    }
                }
            }
        }

        if( exists( abs_disk_path ) )
        {
            boost::filesystem::remove( abs_disk_path );
        }
    }
}

// TODO: Simply put, I believe there is a bug in emscripten's ___syscall_newfstatat that is called by fs::exists, that will trigger in Debug builds
//       because there assertions are included. It doesn't seem to be a real problem, but a false positive "assert()", so testing on files cannot proceed
//       in Debug mode. See https://github.com/emscripten-core/emscripten/issues/17660 for a related open ticket.
SCENARIO( "save and load results in identical state", "[db][fs]")
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database.filesystem", "network", "event_store"/*nw->select_node() fires an event*/, "option_store" ); // TODO: option_store needed because Kmap::load() applies options after loading. That whole dependency needs to be sorted out.

    GIVEN( "minimal state" )
    {
        auto& km = Singleton::instance();
        // Using these helpers to ensure always drawing on the latest component (which are destroyed by reloads).
        auto const db = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Database >() ); };
        auto const nw = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Network >() ); };
        auto initialized_coms = km.component_store().all_initialized_components();
        auto const disk_path = ".db_fs_test.kmap";
        auto const abs_disk_path = com::kmap_root_dir / disk_path;

        if( exists( abs_disk_path ) )
        {
            boost::filesystem::remove( abs_disk_path );
        }

        GIVEN( "init_db_on_disk" )
        {
            REQUIRE_RES( db()->init_db_on_disk( abs_disk_path ) );
            REQUIRE( exists( abs_disk_path ) );
            REQUIRE( db()->has_delta() ); // At least has command_store delta, given current db.fs dependence.

            GIVEN( "flush_delta_to_disk" )
            {
                REQUIRE_RES( db()->flush_delta_to_disk() );
                
                THEN( "!has_delta" )
                {
                    REQUIRE( !db()->has_delta() );
                }

                GIVEN( "load state from db flushed to disk" )
                {
                    REQUIRE_RES( km.load( disk_path, initialized_coms ) );
                    
                    THEN( "!has_delta" )
                    {
                        REQUIRE( !db()->has_delta() );
                    }

                    GIVEN( "create child /1")
                    {
                        auto const c1 = REQUIRE_TRY( nw()->create_child( nw()->root_node(), "1" ) );

                        GIVEN( "flush_delta_to_disk" )
                        {
                            REQUIRE_RES( db()->flush_delta_to_disk() );

                            GIVEN( "erase 1" )
                            {
                                REQUIRE_RES( nw()->erase_node( c1 ) );

                                GIVEN( "flush_delta_to_disk" )
                                {
                                    REQUIRE_RES( db()->flush_delta_to_disk() );

                                    GIVEN( "load state from db flushed to disk" )
                                    {
                                        REQUIRE_RES( km.load( disk_path, initialized_coms ) );

                                        THEN( "!has_delta" )
                                        {
                                            REQUIRE( !db()->has_delta() );
                                        }
                                    }
                                }
                            }
                        }
                        GIVEN( "create child /2")
                        {
                            auto const c2 = REQUIRE_TRY( nw()->create_child( nw()->root_node(), "2" ) );

                            GIVEN( "create_alias( src=/1 dst=/2 )" )
                            {
                                auto const a21 = REQUIRE_TRY( nw()->create_alias( c1, c2 ) );

                                REQUIRE( nw()->exists( a21 ) );
                                REQUIRE( !nw()->fetch_children( c2 ).empty() );

                                GIVEN( "flush_delta_to_disk" )
                                {
                                    REQUIRE_RES( db()->flush_delta_to_disk() );

                                    GIVEN( "erase alias( src=/1 dst=/2 )" )
                                    {
                                        REQUIRE_RES( nw()->erase_node( a21 ) );

                                        GIVEN( "flush_delta_to_disk" )
                                        {
                                            REQUIRE_RES( db()->flush_delta_to_disk() );

                                            REQUIRE( !nw()->exists( a21 ) );
                                            REQUIRE( nw()->fetch_children( c2 ).empty() );

                                            GIVEN( "load state from db flushed to disk" )
                                            {
                                                REQUIRE_RES( km.load( disk_path, initialized_coms ) );

                                                THEN( "alias erased across save/load" )
                                                {
                                                    REQUIRE( !nw()->exists( a21 ) );
                                                    REQUIRE( nw()->fetch_children( c2 ).empty() );
                                                    REQUIRE_RES( nw()->select_node( c2 ) );
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    GIVEN( "flush_delta_to_disk" )
                    {
                        REQUIRE_RES( db()->flush_delta_to_disk() );

                        GIVEN( "load state from db flushed to disk" )
                        {
                            REQUIRE_RES( km.load( disk_path, initialized_coms ) );

                            THEN( "!has_delta" )
                            {
                                REQUIRE( !db()->has_delta() );
                            }
                        }
                    }
                }
            }

            if( exists( abs_disk_path ) )
            {
                boost::filesystem::remove( abs_disk_path );
            }
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
