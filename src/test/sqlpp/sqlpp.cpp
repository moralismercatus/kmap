/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <sqlpp11/sqlpp11.h>
#include <sqlite3.h>

namespace fs = boost::filesystem;
namespace utf = boost::unit_test;

namespace kmap::test {

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( sqlpp11 
                     , 
                     * utf::depends_on( "sqlite3_ts" ) )
                      
BOOST_AUTO_TEST_CASE( open )
{
    BOOST_TEST( true );
}

BOOST_AUTO_TEST_SUITE_END( /* sqlpp11 */ )

} // namespace kmap::test