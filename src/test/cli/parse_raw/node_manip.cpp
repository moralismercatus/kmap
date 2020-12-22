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
BOOST_AUTO_TEST_SUITE( cli )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( parse_raw )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( node_manip )

BOOST_AUTO_TEST_CASE( move_vertical
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    create_lineages( "1"
                   , "2"
                   , "3.4" );

    BOOST_TEST( kmap.create_alias( nodes[ "/1" ]
                                 , nodes[ "/3" ] ) );
    BOOST_TEST( kmap.create_alias( nodes[ "/2" ]
                                 , nodes[ "/3" ] ) );
    BOOST_TEST( kmap.select_node( nodes[ "/3.2" ] ) );
    // 3.[4,1,2]
    BOOST_TEST( cli.parse_raw( ":move.up" ).result == succ );
    // 3.[4,2,1]
    BOOST_TEST( cli.parse_raw( ":move.up" ).result == succ );
    // 3.[2,4,1]
    BOOST_TEST( cli.parse_raw( ":move.up" ).result == fail );
    // 3.[2,4,1]
    BOOST_TEST( cli.parse_raw( ":move.down" ).result == succ );
    // 3.[4,2,1]
    BOOST_TEST( cli.parse_raw( ":move.down" ).result == succ );
    // 3.[4,1,2]
    BOOST_TEST( cli.parse_raw( ":move.down" ).result == fail );
}

BOOST_AUTO_TEST_CASE( sort_children_with_alias
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;

    create_lineages( "1.3"
                   , "1.2"
                   , "4" );

    BOOST_TEST( kmap.create_alias( nodes[ "/4" ]
                                 , nodes[ "/1" ] ) );
    kmap.select_node( nodes[ "/1" ] );
    BOOST_TEST( cli.parse_raw( ":sort.children" ).result == succ );
    BOOST_TEST( ( kmap.fetch_children_ordered( nodes[ "/1" ] ) == std::vector< Uuid >{ nodes[ "/1.2" ]
                                                                                     , nodes[ "/1.3" ] 
                                                                                     , nodes[ "/1.4" ] } ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/1" ] );
}

BOOST_AUTO_TEST_SUITE_END( /* node_manip */ )
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test