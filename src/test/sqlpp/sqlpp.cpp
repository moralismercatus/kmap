/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <sqlpp11/sqlpp11.h>

namespace fs = boost::filesystem;
namespace utf = boost::unit_test;

namespace kmap::test {

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( sqlpp11 
                     , 
                     * utf::depends_on( "sqlite3_ts" ) )
                      
BOOST_AUTO_TEST_CASE( open )
{
    auto const db_path = "/kmap/db_is_working.db";
    auto cfg = sql::connection_config{};
    cfg.path_to_database = db_path;
    cfg.flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    ::sqlite3* sqlite;
    auto const rc = sqlite3_open_v2( cfg.path_to_database.c_str()
                                   , &sqlite
                                   , cfg.flags
                                   , cfg.vfs.empty() ? nullptr : cfg.vfs.c_str() );

    BOOST_TEST( rc == SQLITE_OK );

    if( file_exists( db_path ) )
    {
        fs::remove( db_path );
    }
}

BOOST_AUTO_TEST_SUITE_END( /* sqlpp11 */ )

} // namespace kmap::test