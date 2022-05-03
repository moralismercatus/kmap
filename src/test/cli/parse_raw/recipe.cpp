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
BOOST_AUTO_TEST_SUITE( /*cli/parse_raw*/recipe )

BOOST_AUTO_TEST_CASE( /*cli/parse_raw/recipe*/create
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();

    BOOST_TEST( cli.parse_raw( ":create.recipe 1" )  );
    BOOST_TEST( kmap.exists( "/recipes.1.step" ) );
    BOOST_TEST( !kmap.exists( "/recipes.1.steps" ) );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/recipes.1" );
}

BOOST_AUTO_TEST_CASE( /*cli/parse_raw/recipe*/create_step_empty_body
                    , 
                    * utf::depends_on( "cli/parse_raw/recipe/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();

    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.recipe 1" )  );
    BOOST_TEST_REQUIRE( kmap.absolute_path_flat( kmap.selected_node() ) == "/recipes.1" );

    BOOST_TEST( cli.parse_raw( ":create.step 2" )  );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/recipes.1.steps.2" );
    BOOST_TEST( !kmap.exists( "/recipes.1.step" ) );
    BOOST_TEST( kmap.exists( "/recipes.2" ) );
    BOOST_TEST( kmap.exists( "/recipes.2.step" ) );
    BOOST_TEST( kmap.resolve( kmap.selected_node() ) == *kmap.fetch_leaf( "/recipes.2" ) );
}

BOOST_AUTO_TEST_CASE( /*cli/parse_raw/recipe*/create_step_nonempty_body
                    , 
                    * utf::depends_on( "cli/parse_raw/recipe/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();

    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.recipe 1" )  );
    BOOST_TEST_REQUIRE( kmap.absolute_path_flat( kmap.selected_node() ) == "/recipes.1" );

    BOOST_TEST( !kmap.update_body( nodes[ "/recipes.1.step" ], "content" ).has_error() );

    BOOST_TEST( cli.parse_raw( ":create.step 2" )  );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/recipes.1" );
    BOOST_TEST( kmap.exists( "/recipes.1.step" ) );
    BOOST_TEST( !kmap.exists( "/recipes.1.steps" ) );
    BOOST_TEST( !kmap.exists( "/recipes.2" ) );
}

BOOST_AUTO_TEST_CASE( /*cli/parse_raw/recipe*/create_in_category
                    , 
                    * utf::depends_on( "cli/parse_raw/recipe/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();

    BOOST_TEST( cli.parse_raw( ":create.recipe 1" ) );
    BOOST_TEST( cli.parse_raw( ":create.recipe prereq" ) );
    create_lineages( "recipes.category" );
    BOOST_TEST( kmap.select_node( nodes[ "/recipes.category" ] ) );

    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.recipe 2" )  );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.recipe 3" )  );
    BOOST_TEST_REQUIRE( kmap.exists( "/recipes.category.2" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/recipes.category.3" ) );
    BOOST_TEST_REQUIRE( kmap.selected_node() == nodes[ "/recipes.category.3" ] );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":add.prerequisite category.2" )  );
    BOOST_TEST_REQUIRE( kmap.selected_node() == nodes[ "/recipes.category.3" ] );
    BOOST_TEST_REQUIRE( kmap.exists( "/recipes.category.3.prerequisites.2" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/recipes.category.3.prerequisites.2.step" ) );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":add.step category.2" )  );
    BOOST_TEST_REQUIRE( !kmap.exists( "/recipes.category.3.step" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/recipes.category.3.steps" ) );
    BOOST_TEST_REQUIRE( kmap.exists( "/recipes.category.3.steps.2" ) );
    BOOST_TEST_REQUIRE( kmap.selected_node() == nodes[ "/recipes.category.3" ] );
    BOOST_TEST_REQUIRE( cli.parse_raw( ":create.prerequisite 4" )  );

    BOOST_TEST( kmap.select_node( "/recipes.category.3.steps" ) );
}

BOOST_AUTO_TEST_SUITE_END( /*cli/parse_raw/recipe*/)
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test