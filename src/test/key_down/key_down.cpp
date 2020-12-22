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

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( on_key_down 
                     ,
                     * utf::depends_on( "kmap_iface" ) 
                     * utf::depends_on( "network" ) )
BOOST_AUTO_TEST_SUITE_END( /* on_key_down */ )

} // namespace kmap::test