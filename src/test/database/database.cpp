/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../db.hpp"
#include "../master.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( database 
                     , 
                     * utf::depends_on( "sqlpp11" ) )
                      
BOOST_AUTO_TEST_CASE( placeholder )
{
    BOOST_TEST( true );
}

BOOST_AUTO_TEST_SUITE_END( /* database */ )

} // namespace kmap::test