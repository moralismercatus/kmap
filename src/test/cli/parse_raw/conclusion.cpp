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
BOOST_AUTO_TEST_SUITE( conclusion )

BOOST_AUTO_TEST_CASE( create
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();

    BOOST_TEST( cli.parse_raw( ":create.conclusion 1" ) );
    BOOST_TEST( kmap.exists( "/conclusions.1.assertion" ) );
    BOOST_TEST( !kmap.exists( "/conclusions.1.premises" ) );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/conclusions.1" );
}

BOOST_AUTO_TEST_CASE( create_step_empty_body
                    , 
                    * utf::depends_on( "cli/parse_raw/conclusion/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();

    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.conclusion 1" ) );
    BOOST_TEST_REQUIRE( kmap.absolute_path_flat( kmap.selected_node() ) == "/conclusions.1" );

    BOOST_TEST( cli.parse_raw( ":create.premise 2" ) );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/conclusions.1.premises.2" );
    BOOST_TEST( !kmap.exists( "/conclusions.1.assertion" ) );
    BOOST_TEST( kmap.exists( "/conclusions.2" ) );
    BOOST_TEST( kmap.exists( "/conclusions.2.assertion" ) );
    BOOST_TEST( kmap.resolve( kmap.selected_node() ) == *kmap.fetch_leaf( "/conclusions.2" ) );
}

BOOST_AUTO_TEST_CASE( create_premise_nonempty_body
                    , 
                    * utf::depends_on( "cli/parse_raw/conclusion/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();

    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.conclusion 1" ) );
    BOOST_TEST_REQUIRE( kmap.absolute_path_flat( kmap.selected_node() ) == "/conclusions.1" );

    kmap.update_body( nodes[ "/conclusions.1.assertion" ]
                    , "content" );

    BOOST_TEST( cli.parse_raw( ":create.premise 2" ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/conclusions.1" ] );
    BOOST_TEST( kmap.exists( "/conclusions.1.assertion" ) );
    BOOST_TEST( !kmap.exists( "/conclusions.1.premises" ) );
    BOOST_TEST( !kmap.exists( "/conclusions.2" ) );
}

BOOST_AUTO_TEST_CASE( create_in_category
                    , 
                    * utf::depends_on( "cli/parse_raw/conclusion/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();

    BOOST_TEST( cli.parse_raw( ":create.conclusion 1" ) );
    BOOST_TEST( cli.parse_raw( ":create.conclusion object" ) );
    create_lineages( "conclusions.category" );
    BOOST_TEST( kmap.select_node( nodes[ "/conclusions.category" ] ) );

    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.conclusion 2" ) );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.conclusion 3" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/conclusions.category.2" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/conclusions.category.3" ) );
    BOOST_TEST_REQUIRE( kmap.selected_node() == nodes[ "/conclusions.category.3" ] );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":add.objection category.2" ) );
    BOOST_TEST_REQUIRE( kmap.selected_node() == nodes[ "/conclusions.category.3" ] );
    BOOST_TEST_REQUIRE( kmap.exists( "/conclusions.category.3.objections.2" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/conclusions.category.3.objections.2.assertion" ) );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":add.premise category.2" ) );
    BOOST_TEST_REQUIRE( !kmap.exists( "/conclusions.category.3.assertion" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/conclusions.category.3.premises" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/conclusions.category.3.premises.2" ) );
    BOOST_TEST_REQUIRE( kmap.selected_node() == nodes[ "/conclusions.category.3" ] );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.objection 4" ) );

    BOOST_TEST( kmap.select_node( "/conclusions.category.3.premises" ) );
}

BOOST_AUTO_TEST_SUITE_END( /* conclusion */)
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test