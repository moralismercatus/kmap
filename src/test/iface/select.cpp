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

// TODO: I think we don't want this nested under kmap_iface; rather, we want kmap iface to be a dependency (depends_on()) where relevant.
BOOST_AUTO_TEST_SUITE( kmap_iface )

BOOST_AUTO_TEST_CASE( is_lineal
                    ,
                    * utf::depends_on( "kmap_iface/create_node" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1.2"
                   , "3" );
    
    BOOST_TEST( kmap.is_lineal( nodes[ "/" ]
                              , nodes[ "/" ] ) );
    BOOST_TEST( kmap.is_lineal( nodes[ "/" ]
                              , nodes[ "/1" ] ) );
    BOOST_TEST( kmap.is_lineal( nodes[ "/" ]
                              , nodes[ "/1.2" ] ) );
    BOOST_TEST( !kmap.is_lineal( nodes[ "/1" ]
                               , nodes[ "/" ] ) );
    BOOST_TEST( !kmap.is_lineal( nodes[ "/1.2" ]
                               , nodes[ "/" ] ) );
    BOOST_TEST( !kmap.is_lineal( nodes[ "/1" ]
                               , nodes[ "/3" ] ) );
}

BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test