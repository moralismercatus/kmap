/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../master.hpp"

#include "com/alias/alias.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

// TODO: I think we don't want this nested under kmap_iface; rather, we want kmap iface to be a dependency (depends_on()) where relevant.
BOOST_AUTO_TEST_SUITE( kmap_iface )
BOOST_AUTO_TEST_SUITE( create_alias ) // TODO: This suite should rather be named "alias" alone, as it includes deletion, movement, etc. checks as well.

BOOST_AUTO_TEST_CASE( leaf
                    ,
                    * utf::depends_on( "kmap_iface/create_node" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/2" ] ) );
    BOOST_TEST( nw->exists( "/2.1" ) );
    BOOST_TEST( nw->fetch_children( nodes[ "/2.1" ] ).empty() );
    BOOST_TEST( nw->resolve( nodes[ "/2.1" ] ) == nodes[ "/1" ] );

    BOOST_TEST( kmap.select_node( nodes[ "/2.1" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/2.1" ] );
}

BOOST_AUTO_TEST_CASE( delete_alias
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/leaf" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/2" ] ) );
    BOOST_TEST( nw->erase_node( nodes[ "/2.1" ] ) );
    BOOST_TEST( !nw->exists( "/2.1" ) );
    BOOST_TEST( nw->fetch_children( nodes[ "/2" ] ).empty() );
    BOOST_TEST( nw->exists( "/1" ) );

    BOOST_TEST( kmap.select_node( nodes[ "/2" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/2" ] );
}

BOOST_AUTO_TEST_CASE( alias_from_alias
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/leaf" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1.2"
                   , "3"
                   , "4" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/3" ] ) );
    BOOST_TEST( nw->create_alias( nodes[ "/3.1" ], nodes[ "/4" ] ) );
    BOOST_TEST( nw->resolve( nodes[ "/4.1" ] ) == nodes[ "/1" ] );
    BOOST_TEST( nw->resolve( nodes[ "/4.1.2" ] ) == nodes[ "/1.2" ] );
}

BOOST_AUTO_TEST_CASE( alias_has_alias_child
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/leaf" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1.2"
                   , "3"
                   , "4" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/3" ] ) );
    BOOST_TEST( nw->create_alias( nodes[ "/3" ], nodes[ "/4" ] ) );
    BOOST_TEST( nw->resolve( nodes[ "/4.3.1" ] ) == nodes[ "/1" ] );

    BOOST_TEST( kmap.select_node( nodes[ "/4" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/4" ] );
    BOOST_TEST( kmap.select_node( nodes[ "/4.3" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/4.3" ] );
    BOOST_TEST( kmap.select_node( nodes[ "/4.3.1.2" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/4.3.1.2" ] );
}

BOOST_AUTO_TEST_CASE( has_child
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/leaf" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1.2"
                   , "3"
                   , "4" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/3" ] ) );
    BOOST_TEST( nw->exists( "/3.1.2" ) );
    BOOST_TEST( nw->fetch_children( nodes[ "/3.1" ] ).size() == 1 );
    BOOST_TEST( nw->resolve( nodes[ "/3.1" ] ) == nodes[ "/1" ] );
    BOOST_TEST( kmap.select_node( nodes[ "/3.1" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/3.1" ] );
}

BOOST_AUTO_TEST_CASE( update_alias_create_child
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/has_child" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/2" ] ) );
    create_lineages( "1.3" );
    BOOST_TEST( nw->exists( "/2.1.3" ) );
    BOOST_TEST( kmap.select_node( nodes[ "/2.1.3" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/2.1.3" ] );
}

BOOST_AUTO_TEST_CASE( update_alias_delete_child
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/has_child" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1.2"
                   , "3" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/3" ] ) );
    BOOST_TEST( nw->erase_node( nodes[ "/1.2" ] ) );
    BOOST_TEST( !nw->exists( "/3.1.2" ) );
    BOOST_TEST( kmap.select_node( nodes[ "/3.1" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/3.1" ] );
}

BOOST_AUTO_TEST_CASE( update_alias_delete_source
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/has_child" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2" );

    BOOST_TEST( nw->create_alias( nodes[ "/1" ], nodes[ "/2" ] ) );
    BOOST_TEST( nw->erase_node( nodes[ "/1" ] ) );
    BOOST_TEST( !nw->exists( "/1" ) );
    BOOST_TEST( !nw->exists( "/2.1" ) );
    BOOST_TEST( kmap.select_node( nodes[ "/2" ] ) );
    BOOST_TEST( nw->selected_node() == nodes[ "/2" ] );
}

BOOST_AUTO_TEST_CASE( create_alias_nested_outside_in
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/has_child" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2" 
                   , "3" );

    BOOST_TEST( nw->create_alias( nodes[ "/3" ], nodes[ "/2" ] ) );
    BOOST_TEST( nw->create_alias( nodes[ "/2" ], nodes[ "/1" ] ) );
    BOOST_TEST( kmap.select_node( nodes[ "/1" ] ) );
    BOOST_TEST( nw->travel_right() );
    BOOST_TEST( nw->travel_right() );
    BOOST_TEST( nw->selected_node() == nodes[ "/1.2.3" ] );
}

BOOST_AUTO_TEST_CASE( create_alias_nested_inside_out
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/has_child" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2" 
                   , "3" );

    BOOST_TEST( nw->create_alias( nodes[ "/2" ], nodes[ "/1" ] ) );
    BOOST_TEST( nw->create_alias( nodes[ "/3" ], nodes[ "/2" ] ) );
    BOOST_TEST( kmap.select_node( nodes[ "/1" ] ) );
    BOOST_TEST( nw->travel_right() );
    BOOST_TEST( nw->travel_right() );
    BOOST_TEST( nw->selected_node() == nodes[ "/1.2.3" ] );
}

BOOST_AUTO_TEST_CASE( create_alias_from_alias
                    ,
                    * utf::depends_on( "kmap_iface/create_alias/has_child" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const astore = KTRYE( kmap.fetch_component< com::AliasStore >() );
    auto const nodes = kmap.node_fetcher();

    create_lineages( "1"
                   , "2" 
                   , "3" );

    BOOST_TEST( nw->create_alias( nodes[ "/2" ], nodes[ "/1" ] ) ); // Alias 2 to 1
    BOOST_TEST( nw->create_alias( nodes[ "/3" ], nodes[ "/1.2" ] ) ); // Alias 3 to the alias 2 (1.2).
}

BOOST_AUTO_TEST_SUITE_END( /*create_alias*/ )
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test