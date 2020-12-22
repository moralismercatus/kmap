/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../master.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( parse_raw
                     ,
                     * utf::depends_on( "cli/parser" )
                     * utf::fixture< ClearMapFixture >() )
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )

} // namespace kmap::test