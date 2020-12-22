/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( cli
                     ,
                     * utf::depends_on( "kmap_iface" )
                     * utf::fixture< ClearMapFixture >() )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test