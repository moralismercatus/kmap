
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../canvas.hpp"
#include "../../kmap.hpp"
#include "../master.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( kmap_iface )
BOOST_AUTO_TEST_SUITE( canvas ) 

BOOST_AUTO_TEST_CASE( subdivide
                    ,
                    // * utf::depends_on( "kmap_iface/create_node" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto canvas = Canvas{ kmap };

    // BOOST_TEST( canvas.subdivide( "" ) );
}

BOOST_AUTO_TEST_SUITE_END( /* canvas */ )
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test