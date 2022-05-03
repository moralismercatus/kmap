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
BOOST_AUTO_TEST_SUITE( node_manip 
                     ,
                     * utf::depends_on( "kmap_iface/create_node" ) )
                      
BOOST_AUTO_TEST_CASE( move_node
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();

    auto const c1 = kmap.create_child( kmap.root_node_id()
                                     , "1" );
    auto const c2 = kmap.create_child( kmap.root_node_id()
                                     , "2" );
    BOOST_TEST( kmap.move_node( c1.value()
                              , c2.value() ) );

    BOOST_TEST( !kmap.exists( "/1" ) );
    BOOST_TEST( kmap.exists( "/2.1" ) );
}

BOOST_AUTO_TEST_CASE( move_node_into_descendant
                    ,
                    * utf::depends_on( "kmap_iface/node_manip/move_node" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1.2" );

    BOOST_TEST( false == kmap.move_node( nodes[ "1" ]
                                       , nodes[ "2" ] )
                       . has_value() );
    BOOST_TEST( kmap.exists( "/1.2" ) );
}

BOOST_AUTO_TEST_CASE( move_node_alias
                    ,
                    * utf::depends_on( "kmap_iface/create_alias" )
                    * utf::depends_on( "kmap_iface/node_manip/move_node" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();

    auto const c1 = kmap.create_child( kmap.root_node_id()
                                     , "1" );
    auto const c2 = kmap.create_child( kmap.root_node_id()
                                     , "2" );
    auto c2_c1 = kmap.create_alias( c1.value()
                                  , c2.value() );
    BOOST_REQUIRE( c2_c1 );
    BOOST_TEST( kmap.exists( "/1" ) );
    BOOST_TEST( kmap.exists( "/2" ) );
    BOOST_TEST( kmap.exists( "/2.1" ) );
    BOOST_TEST( c1.value() == kmap.resolve( *kmap.fetch_leaf( "/2.1" ) ) );

    auto const c3 = kmap.create_child( kmap.root_node_id()
                                     , "3" );
    BOOST_TEST( kmap.move_node( c2_c1.value(), c3.value() ) );
    BOOST_TEST( !kmap.exists( "/2.1" ) );
    BOOST_TEST( kmap.exists( "/3.1" ) );

    BOOST_TEST( kmap.select_node( *kmap.fetch_leaf( "/2" ) ) );
    BOOST_TEST( c2.value() == kmap.selected_node() );
    BOOST_TEST( kmap.select_node( *kmap.fetch_leaf( "/3.1" ) ) );
    BOOST_TEST( c1.value() == kmap.resolve( kmap.selected_node() ) );
}

BOOST_AUTO_TEST_CASE( move_node_duplicate_heading
                    ,
                    * utf::depends_on( "kmap_iface/node_manip/move_node" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2.5"
                   , "3.5" );

    BOOST_TEST( kmap.move_node( nodes[ "2.5" ], nodes[ "1" ] ) );
    BOOST_TEST( !kmap.move_node( nodes[ "3.5" ], nodes[ "1" ] ) );

    BOOST_TEST( kmap.fetch_children( nodes[ "1" ] ).size() == 1 );
}

BOOST_AUTO_TEST_CASE( copy_body
                    ,
                    // TODO: , depends_on( "kmap_iface/update_body" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nodes = kmap.node_fetcher();
    auto const content_1 = "content_1";
    auto const content_2 = "content_2";

    create_lineages( "1"
                   , "2" );
    
    BOOST_TEST( !kmap.update_body( nodes[ "1" ], content_1 ).has_error() );
    BOOST_TEST( !kmap.update_body( nodes[ "2" ], content_2 ).has_error() );
    BOOST_TEST( !kmap.copy_body( nodes[ "1" ], nodes[ "2" ] ).has_error() );
    BOOST_TEST( kmap.fetch_body( nodes[ "1" ] ).has_value() );
    BOOST_TEST( content_1 == kmap.fetch_body( nodes[ "1" ] ).value() );
    BOOST_TEST( content_1 == kmap.fetch_body( nodes[ "2" ] ).value() );
}

BOOST_AUTO_TEST_CASE( move_body
                    ,
                    // TODO: , depends_on( "kmap_iface/update_body" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nodes = kmap.node_fetcher();
    auto const content_1 = "content_1";
    auto const content_2 = "content_2";

    create_lineages( "1"
                   , "2" );
    
    kmap.update_body( nodes[ "1" ]
                    , content_1 );
    kmap.update_body( nodes[ "2" ]
                    , content_2 );
    kmap.copy_body( nodes[ "1" ]
                  , nodes[ "2" ] );
    
    BOOST_TEST( kmap.fetch_body( nodes[ "1" ] ).has_value() );
    BOOST_TEST( content_1 == kmap.fetch_body( nodes[ "1" ] ).value() );
    BOOST_TEST( content_1 == kmap.fetch_body( nodes[ "2" ] ).value() );
}

BOOST_AUTO_TEST_SUITE_END( /* node_manip */ )
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test