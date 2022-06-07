/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../network.hpp"
#include "../master.hpp"
#include "js_iface.hpp"
#include "canvas.hpp"
#include "test/util.hpp"

#include <boost/test/unit_test.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace kmap;
using namespace kmap::test;

struct NetworkFixture
{
	std::string file;
	uint32_t line;

    NetworkFixture( std::string const& curr_file = __builtin_FILE()
                  , uint32_t const curr_line = __builtin_LINE() )
        : file{ curr_file }
        , line{ curr_line }
    {
        try
        {
            auto& kmap = Singleton::instance();

            kmap.init_root_node();
            kmap.init_database();
            KTRYE( kmap.set_up_db_root() );
            kmap.init_option_store();
            kmap.init_event_store();
            kmap.init_canvas();
            KTRYE( kmap.canvas().reset() );
        }
        catch( std::exception const& e )
        {
            fmt::print( stderr, "Fixture ctor failed: {}|{}\n", file, line );
            std::cerr << e.what() << '\n';
            throw;
        }
    }
    ~NetworkFixture()
    {
        try
        {
            auto& kmap = Singleton::instance();

            kmap.init_root_node();
            kmap.clear_canvas();
            kmap.clear_event_store();
            kmap.clear_option_store();
            kmap.clear_database();
        }
        catch( std::exception const& e )
        {
            fmt::print( stderr, "Fixture dtor failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
            std::cerr << e.what() << '\n';
            std::terminate();
        }
    }
};

SCENARIO( "network testing", "[network][env]" )
{
    auto& kmap = Singleton::instance();
    KMAP_LOG_LINE();
    NetworkFixture network_fixture{};
    KMAP_LOG_LINE();
    auto nw = Network{ kmap, kmap.canvas().network_pane() };
    KMAP_LOG_LINE();

    GIVEN( "blank network" )
    {
        auto const root_id = kmap.root_node_id();
        auto const root_title = "Root";

        WHEN( "create initial node" )
        {
            REQUIRE( succ( nw.create_node( root_id, root_title ) ) );

            THEN( "node exists" )
            {
                REQUIRE( nw.exists( root_id ) );
            }
            THEN( "title matches" )
            {
                REQUIRE( succ( nw.fetch_title( root_id ) ) );
                REQUIRE( nw.fetch_title( root_id ).value() == root_title );
            }
            THEN( "childless" )
            {
                REQUIRE( nw.children( root_id ).empty() );
            }
            THEN( "parentless" )
            {
                REQUIRE( fail( nw.fetch_parent( root_id ) ) );
            }

            REQUIRE_RES( nw.erase_node( root_id ) );
        }

    }
    GIVEN( "root node" )
    {
        auto const root_id = gen_uuid();
        auto const root_title = "Root";

        REQUIRE( succ( nw.create_node( root_id, root_title ) ) );

        WHEN( "create child" )
        {
            auto const child_id = gen_uuid();
            auto const child_title = "Child";

            REQUIRE( succ( nw.create_node( child_id, child_title ) ) );
            REQUIRE( fail( nw.edge_exists( root_id, child_id ) ) );
            REQUIRE( succ( nw.add_edge( root_id, child_id ) ) );

            THEN( "child exists" )
            {
                REQUIRE( nw.children( root_id ).size() == 1 );
                REQUIRE( nw.children( root_id ).at( 0 ) == child_id );
                REQUIRE( nw.fetch_title( child_id ).value() == child_title );
            }
            THEN( "edge between root and child" )
            {
                REQUIRE( nw.edge_exists( root_id, child_id ) );
            }

            REQUIRE( succ( nw.remove_edge( root_id, child_id ) ) );
            REQUIRE( succ( nw.erase_node( child_id ) ) );
            REQUIRE( succ( nw.erase_node( root_id ) ) );
        }

        REQUIRE( succ( nw.create_node( root_id, root_title ) ) );
        REQUIRE( succ( nw.erase_node( root_id ) ) );
    }
}

#if 0
namespace utf = boost::unit_test;

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( network
                     ,
                     * utf::label( "env" ) )

BOOST_AUTO_TEST_CASE( create_node )
{
    auto& kmap = Singleton::instance();
    auto nw = Network{ kmap.canvas().network_pane() };

    BOOST_TEST( nw.exists( nw.create_node( { 0x0 }, "0" ) ) );
    BOOST_TEST( nw.nodes().size() == 1 );
    BOOST_TEST( nw.exists( nw.create_node( { 0x1 }, "a" ) ) );
    BOOST_TEST( nw.nodes().size() == 2 );
    BOOST_TEST( nw.exists( nw.create_node( { 0x2 }, "b" ) ) );
    BOOST_TEST( nw.nodes().size() == 3 );
    BOOST_TEST( nw.exists( nw.create_node( { 0x3 }, "c" ) ) );
    BOOST_TEST( nw.nodes().size() == 4 );
}

BOOST_AUTO_TEST_CASE( add_edge
                    ,
                    * utf::depends_on( "network/create_node" ) )
{
    auto& kmap = Singleton::instance();
    auto nw = Network{ kmap.canvas().network_pane() };
 
    nw.create_node( { 0x0 }, "0" );
    nw.create_node( { 0x1 }, "a" );
    nw.create_node( { 0x2 }, "b" );
    nw.create_node( { 0x3 }, "c" );

    nw.add_edge( { 0x0 }, { 0x1 } );
    BOOST_TEST( nw.is_child( { 0x0 }, Uuid{ 0x1 } ) );
    nw.add_edge( { 0x1 }, { 0x2 } );
    BOOST_TEST( nw.is_child( { 0x1 }, Uuid{ 0x2 } ) );
    nw.add_edge( { 0x1 }, { 0x3 } );
    BOOST_TEST( nw.is_child( { 0x1 }, Uuid{ 0x3 } ) );
}

BOOST_AUTO_TEST_SUITE_END( /* network */ )
} // namespace kmap::test
#endif // 0
