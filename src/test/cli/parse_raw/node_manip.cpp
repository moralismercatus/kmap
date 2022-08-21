/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../master.hpp"

#include "com/alias/alias.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"

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
    auto const cli = KTRYE( kmap.fetch_component< com::Cli >() );
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2"
                   , "3.4" );

    BOOST_TEST( nw->alias_store().create_alias( nodes[ "/1" ]
                                    , nodes[ "/3" ] ) );
    BOOST_TEST( nw->alias_store().create_alias( nodes[ "/2" ]
                                    , nodes[ "/3" ] ) );
    BOOST_TEST( kmap.select_node( nodes[ "/3.2" ] ) );
    // 3.[4,1,2]
    BOOST_TEST( cli->parse_raw( ":move.up" ) );
    // 3.[4,2,1]
    BOOST_TEST( cli->parse_raw( ":move.up" ) );
    // 3.[2,4,1]
    BOOST_TEST( cli->parse_raw( ":move.up" ) );
    // 3.[2,4,1]
    BOOST_TEST( cli->parse_raw( ":move.down" ) );
    // 3.[4,2,1]
    BOOST_TEST( cli->parse_raw( ":move.down" ) );
    // 3.[4,1,2]
    BOOST_TEST( cli->parse_raw( ":move.down" ) );
}

BOOST_AUTO_TEST_CASE( sort_children_with_alias
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const cli = KTRYE( kmap.fetch_component< com::Cli >() );
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1.3"
                   , "1.2"
                   , "4" );

    BOOST_TEST( nw->alias_store().create_alias( nodes[ "/4" ], nodes[ "/1" ] ) );
    BOOST_TEST( kmap.select_node( nodes[ "/1" ] ) );
    BOOST_TEST( cli->parse_raw( ":sort.children" )  );
    BOOST_TEST( ( ( view::make( nodes[ "/1" ] ) | view::child | view::to_node_set( kmap ) | act::order( kmap ) ) ) == std::vector< Uuid >{ nodes[ "/1.2" ]
                                                                                                                                         , nodes[ "/1.3" ] 
                                                                                                                                         , nodes[ "/1.4" ] } ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/1" ] );
}

BOOST_AUTO_TEST_SUITE_END( /* node_manip */ )
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test