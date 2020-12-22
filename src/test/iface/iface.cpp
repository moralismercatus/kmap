/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( kmap_iface 
                     , 
                     * utf::depends_on( "database" )
                     * utf::depends_on( "filesystem" )
                     * utf::depends_on( "network" )
                     * utf::depends_on( "path" ) )
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test