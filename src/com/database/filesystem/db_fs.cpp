/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/database/filesystem/db_fs.hpp>

#include <com/cmd/cclerk.hpp>
#include <com/database/db.hpp>
#include <com/database/table_decl.hpp>
#include <com/filesystem/filesystem.hpp>
#include <error/filesystem.hpp>
#include <io.hpp>
#include <kmap.hpp>
#include <test/util.hpp>
#include <util/result.hpp>
#include <util/signal_exception.hpp>
#include <utility.hpp>

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>
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
{
}

auto DatabaseFilesystem::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto DatabaseFilesystem::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

SCENARIO( "saved data mirrors runtime data", "[db][fs][save][load]")
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

SCENARIO( "save and load results in identical state for minimal state", "[db][fs][save][load]")
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database", "database.filesystem", "network" );

    GIVEN( "minimal state" )
    {
        auto& km = Singleton::instance();
        // Using these helpers to ensure always drawing on the latest component (which are destroyed by reloads).
        auto const db = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Database >() ); };
        auto const nw = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Network >() ); };
        auto initialized_coms = km.component_store().all_initialized_components();
        auto const disk_path = ".db_fs_test.kmap";
        auto const abs_disk_path = com::kmap_root_dir / disk_path;

        if( exists( abs_disk_path ) ) { boost::filesystem::remove( abs_disk_path ); }

        GIVEN( "init_db_on_disk" )
        {
            REQUIRE_TRY( db()->init_db_on_disk( abs_disk_path ) );
            REQUIRE( exists( abs_disk_path ) );
            REQUIRE( db()->has_delta() ); // At least has root_node delta (creation of root node), given current network dependence.

            // TODO:
            // auto const n1 = REQUIRE_TRY( nw()->create_child( nw()->root_node(), "1" ) );
            // auto const a21 = REQUIRE_TRY( nw()->create_alias( n1, n2 ) );
            // nw->create_alias() // create alias... then ensure that alias exists after load...
            // REQUIRE_TRY( nw->create_alias() );

            GIVEN( "flush_delta_to_disk" )
            {
                REQUIRE_TRY( db()->flush_delta_to_disk() );

                
                THEN( "!has_delta" )
                {
                    REQUIRE( !db()->has_delta() );
                }

                // REQUIRE( nw()->exists( a21 ) );

                GIVEN( "load state from db flushed to disk" )
                {
                    // KM_LOG_ENABLE( "*" );
                    KMAP_LOG_LINE();
                    // KM_LOG_ST_ENABLE();
                    REQUIRE_TRY( km.load( disk_path, initialized_coms ) );
                    // KM_LOG_ST_DISABLE();
                    KMAP_LOG_LINE();
                    REQUIRE( db()->has_file_on_disk() );
                    REQUIRE( !db()->has_delta() );
                    // KM_LOG_DISABLE( "*" );
                    
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

                                    GIVEN( "load state from db flushed to disk" )
                                    {
                                        REQUIRE_RES( km.load( disk_path, initialized_coms ) );

                                        THEN( "nodes and alias still exists across save/load" )
                                        {
                                            REQUIRE( nw()->exists( c1 ) );
                                            REQUIRE( nw()->exists( c2 ) );
                                            REQUIRE( nw()->exists( a21 ) );
                                            REQUIRE( !nw()->fetch_children( c2 ).empty() );
                                            REQUIRE_TRY( nw()->select_node( c2 ) );
                                        }
                                    }
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

            if( exists( abs_disk_path ) ) { boost::filesystem::remove( abs_disk_path ); }
        }
    }
}

// TODO: Simply put, I believe there is a bug in emscripten's ___syscall_newfstatat that is called by fs::exists, that will trigger in Debug builds
//       because there assertions are included. It doesn't seem to be a real problem, but a false positive "assert()", so testing on files cannot proceed
//       in Debug mode. See https://github.com/emscripten-core/emscripten/issues/17660 for a related open ticket.
//       Update: https://github.com/emscripten-core/emscripten/pull/18261 claims to have fixed this issue. TBD.
SCENARIO( "save and load results in identical state for all listed components", "[db][fs][save][load]")
{
    auto const all_components = REQUIRE_TRY( fetch_listed_components() );

    KMAP_COMPONENT_FIXTURE_SCOPED( all_components );

    GIVEN( "minimal state" )
    {
        auto& km = Singleton::instance();
        // Using these helpers to ensure always drawing on the latest component (which are destroyed by reloads).
        auto const db = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Database >() ); };
        auto const nw = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Network >() ); };
        auto initialized_coms = km.component_store().all_initialized_components();
        auto const disk_path = ".db_fs_test.kmap";
        auto const abs_disk_path = com::kmap_root_dir / disk_path;

        if( exists( abs_disk_path ) ) { boost::filesystem::remove( abs_disk_path ); }

        GIVEN( "init_db_on_disk" )
        {
            REQUIRE_TRY( db()->init_db_on_disk( abs_disk_path ) );
            REQUIRE( exists( abs_disk_path ) );
            REQUIRE( db()->has_delta() ); // At least has command.store delta, given current db.fs dependence.

            THEN( "travel.right command has unconditional guard" )
            {
                REQUIRE(( view2::cmd::command_root
                        | view2::direct_desc( "travel.right.guard.unconditional" )
                        | act2::exists( km ) ));
            }

            GIVEN( "flush_delta_to_disk" )
            {
                REQUIRE_TRY( db()->flush_delta_to_disk() );
                
                THEN( "!has_delta" )
                {
                    REQUIRE( !db()->has_delta() );
                }

                GIVEN( "load state from db flushed to disk" )
                {
                    // KM_LOG_ENABLE( "*" );
                    KMAP_LOG_LINE();
                    // KM_LOG_ST_ENABLE();
                    REQUIRE_TRY( km.load( disk_path, initialized_coms ) );
                    // KM_LOG_ST_DISABLE();
                    KMAP_LOG_LINE();
                    THEN( "travel.right command has unconditional guard" )
                    {
                        REQUIRE(( view2::cmd::command_root
                                | view2::direct_desc( "travel.right.guard.unconditional" )
                                | act2::exists( km ) ));
                    }
                    REQUIRE( db()->has_file_on_disk() );
                    REQUIRE( !db()->has_delta() );
                    // KM_LOG_DISABLE( "*" );
                    
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

            if( exists( abs_disk_path ) ) { boost::filesystem::remove( abs_disk_path ); }
        }
    }
}

SCENARIO( "save.as after save.as output DBs are mirrored for all listed components", "[db][fs][save][load]")
{
    auto const all_components = REQUIRE_TRY( fetch_listed_components() );

    KMAP_COMPONENT_FIXTURE_SCOPED( all_components );

    auto& km = Singleton::instance();
    auto initialized_coms = km.component_store().all_initialized_components();
    auto const disk_path_1 = ".db_fs_test.1.kmap";
    auto const disk_path_2 = ".db_fs_test.2.kmap";
    auto const abs_disk_path_1 = com::kmap_root_dir / disk_path_1;
    auto const abs_disk_path_2 = com::kmap_root_dir / disk_path_2;
    auto const save_as = [ & ]( auto const& p )
    {
        auto const db = REQUIRE_TRY( km.fetch_component< com::Database >() );
        REQUIRE_TRY( db->init_db_on_disk( p ) );
        REQUIRE_TRY( db->flush_cache_to_disk() );
    };
    auto const fetch_cache = [ & ]()
    {
        auto const db = REQUIRE_TRY( km.fetch_component< com::Database >() );
        return std::as_const( *db ).cache();
    };

    GIVEN( "map state provided by dependencies: network, option_store, etc." )
    {
        if( exists( abs_disk_path_1 ) ) { boost::filesystem::remove( abs_disk_path_1 ); }
        if( exists( abs_disk_path_2 ) ) { boost::filesystem::remove( abs_disk_path_2 ); }

        GIVEN( "save.as path_1" )
        {
            KM_LOG_ENABLE( "*" );
            // TODO: I think the thing to do is drop parse_raw, as it requries JS. The command should be unit tested separately.
            //       Rather, we should call the functions directly ("init_on_disk, flush_delta").
            save_as( disk_path_1 );
            KM_LOG_DISABLE( "*" );
            REQUIRE_TRY( km.load( disk_path_1, initialized_coms ) );
            auto const cache_post_save_1 = fetch_cache();

            WHEN( "save.as path_2" )
            {
                save_as( disk_path_2 );
                REQUIRE_TRY( km.load( disk_path_2, initialized_coms ) );

                auto const cache_post_save_2 = fetch_cache();

                THEN( "path_1 and path_2 represent identical files" )
                {
                    REQUIRE( cache_post_save_1.tables() == cache_post_save_2.tables() );
                }
            }
        }
    }

    if( exists( abs_disk_path_1 ) ) { boost::filesystem::remove( abs_disk_path_1 ); }
    if( exists( abs_disk_path_2 ) ) { boost::filesystem::remove( abs_disk_path_2 ); }
}

auto DatabaseFilesystem::copy_state( FsPath const& dst )
    -> Result< void >
{
    using boost::system::error_code;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "dst", dst.string() );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( file_exists( dst ), kmap::error_code::filesystem::file_not_found );

    auto ec = error_code{};
    auto const db = KTRY( fetch_component< com::Database >() );
    auto const path = KTRY( db->path() );

    bfs::copy_file( path
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

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "dst", dst.string() );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE_MSG( file_exists( dst ), kmap::error_code::filesystem::file_not_found, io::format( "unable to find file {}", dst.string() ) );

    auto ec = error_code{};
    auto const db = KTRY( fetch_component< com::Database >() );
    auto const path = KTRY( db->path() );

    bfs::rename( path
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
,   std::set({ "database"s, "filesystem"s })
,   "database's filesystem support"
);

} // namespace filesystem_def 
}

} // namespace kmap::com
