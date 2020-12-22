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
BOOST_AUTO_TEST_SUITE( project
                     ,
                     * utf::disabled() ) // TODO: Re-enable test suite. Temporary disabled until reassessment of feature.
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( uncategorized )

BOOST_AUTO_TEST_CASE( /*uncategorized*/create
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    BOOST_TEST_REQUIRE( kmap.selected_node() == kmap.root_node_id() );
    BOOST_TEST( cli.parse_raw( ":create.project 1" ).result == succ );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.1" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.1.problem_statement" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.1.result" ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.1" ] );

    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.1" ] );
    BOOST_TEST( cli.parse_raw( ":create.project 2" ).result == succ );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.2" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.2.problem_statement" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.2.result" ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.2" ] );

    kmap.select_node( nodes[ "/projects.open.inactive.2.problem_statement" ] );
    BOOST_TEST( cli.parse_raw( ":create.project 3" ).result == succ );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.3" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.3.problem_statement" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.3.result" ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.3" ] );

    BOOST_TEST( cli.parse_raw( ":create.project 1" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.project 2" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.project 3" ).result == fail );

    BOOST_TEST( select_each_descendant_test( kmap, nodes[ "/projects.open.inactive" ] ) );
}

BOOST_AUTO_TEST_CASE( /*uncategorized*/create_task
                    , 
                    * utf::depends_on( "cli/parse_raw/project/uncategorized/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;

    cli.parse_raw( ":create.project 1" );
    BOOST_TEST( cli.parse_raw( ":create.task 2" ).result == succ );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.2" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.2.problem_statement" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.2.result" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.1.tasks.open.2" ) );
    BOOST_TEST( kmap.is_alias( nodes[ "/projects.open.inactive.1.tasks.open.2" ] ) );
    BOOST_TEST( kmap.resolve( nodes[ "/projects.open.inactive.1.tasks.open.2" ] ) == nodes[ "/projects.open.inactive.2" ] );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.1.tasks.open.2" ] );

    BOOST_TEST( select_each_descendant_test( kmap, nodes[ "/projects.open.inactive" ] ) );
}

// TODO: `create.sibling.task` should be used for creating a task of the parent project whilst a task is selected.
BOOST_AUTO_TEST_CASE( /*uncategorized*/create_task_from_task
                    , 
                    * utf::depends_on( "cli/parse_raw/project/uncategorized/create_task" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;

    BOOST_TEST( cli.parse_raw( ":create.project 1" ).result == succ );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.1" ] );
    BOOST_TEST( cli.parse_raw( ":create.task 2" ).result == succ );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.1.tasks.open.2" ] );
    BOOST_TEST( cli.parse_raw( ":create.task 3" ).result == succ );
    // TODO: Should be this one?: //BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.1.tasks.open.2.tasks.open.3" ] );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.2.tasks.open.3" ] );

    BOOST_TEST( select_each_descendant_test( kmap, nodes[ "/projects.open.inactive" ] ) );
}

BOOST_AUTO_TEST_CASE( close_project
                    , 
                    * utf::depends_on( "cli/parse_raw/project/uncategorized/create_task" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    BOOST_TEST( cli.parse_raw( ":close.project" ).result == fail );

    cli.parse_raw( ":create.project 1" );

    BOOST_TEST( cli.parse_raw( ":close.project" ).result == fail );

    kmap.update_body( nodes[ "/projects.open.inactive.1.result" ], "content" );

    BOOST_TEST( cli.parse_raw( ":close.project" ).result == succ );
    BOOST_TEST( kmap.exists( "/projects.closed.1" ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.closed.1" ] );

    BOOST_TEST( select_each_descendant_test( kmap, nodes[ "/projects" ] ) );
}

BOOST_AUTO_TEST_CASE( close_project_w_open_task
                    , 
                    * utf::depends_on( "cli/parse_raw/project/uncategorized/close_project" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const fail = CliResultCode::failure;

    cli.parse_raw( ":create.project 1" );
    cli.parse_raw( ":create.task 2" );

    kmap.select_node( nodes[ "/projects.open.inactive.1" ] );

    BOOST_TEST( cli.parse_raw( ":close.project" ).result == fail );
}

BOOST_AUTO_TEST_SUITE_END( /* uncategorized */ )

/******************************************************************************/
/* cli/parse_raw/project/categorized */
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( categorized
                     ,
                     * utf::depends_on( "cli/parse_raw/project/uncategorized" ) )
                      
BOOST_AUTO_TEST_CASE( create
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;

    create_lineages( "projects.open.inactive.cat" );

    kmap.select_node( "projects.open.inactive.cat" );

    BOOST_TEST( cli.parse_raw( ":create.project 1" ).result == succ );

    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.1" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.1.problem_statement" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.1.result" ] ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.cat.1" ] );

    BOOST_TEST( cli.parse_raw( ":create.project 2" ).result == succ );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.2" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.2.problem_statement" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.2.result" ] ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.open.inactive.cat.2" ] );

    BOOST_TEST( select_each_descendant_test( kmap, nodes[ "/projects.open.inactive" ] ) );
}

BOOST_AUTO_TEST_CASE( create_task
                    , 
                    * utf::depends_on( "cli/parse_raw/project/categorized/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;

    create_lineages( "projects.open.inactive.cat" );

    kmap.select_node( "projects.open.inactive.cat" );

    BOOST_TEST( cli.parse_raw( ":create.project 1" ).result == succ );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.1" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.1.problem_statement" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.1.result" ] ) );

    BOOST_TEST( cli.parse_raw( ":create.task 2" ).result == succ );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.2" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.2.problem_statement" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.2.result" ] ) );
    BOOST_TEST( kmap.exists( nodes[ "/projects.open.inactive.cat.1" ] ) );

    BOOST_TEST( select_each_descendant_test( kmap, nodes[ "/projects.open.inactive" ] ) );
}

BOOST_AUTO_TEST_CASE( close_project
                    , 
                    * utf::depends_on( "cli/parse_raw/project/categorized/create" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const nodes = kmap.node_fetcher();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    create_lineages( "projects.open.inactive.cat" );

    kmap.select_node( "projects.open.inactive.cat" );

    BOOST_TEST( cli.parse_raw( ":create.project 1" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":close.project" ).result == fail );
    kmap.update_body( nodes[ "/projects.open.inactive.cat.1.result" ], "content" );
    BOOST_TEST( cli.parse_raw( ":close.project" ).result == succ );
    BOOST_TEST( kmap.exists( "/projects.closed.cat.1" ) );
    BOOST_TEST( kmap.selected_node() == nodes[ "/projects.closed.cat.1" ] );
    BOOST_TEST( cli.parse_raw( ":close.project" ).result == fail );
    BOOST_TEST( !kmap.exists( "/projects.open.inactive.cat" ) );

    BOOST_TEST( select_each_descendant_test( kmap, nodes[ "/projects" ] ) );
}

BOOST_AUTO_TEST_SUITE_END( /* categorized */ )

#if 0
BOOST_AUTO_TEST_CASE( misc // TODO: Break into constituent parts.
                    , 
                    * utf::depends_on( "cli/parse_raw/project/uncategorized" ) 
                    * utf::depends_on( "cli/parse_raw/project/categorized" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    // create p_1
    BOOST_TEST( cli.parse_raw( ":create.project p_1" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":create.project p_1" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.project P 1" ).result == fail );
    BOOST_TEST( kmap.fetch_heading( kmap.selected_node() ).value() == "p_1" );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.open.inactive.p_1" );
    // create p_2 as a task of p_1
    BOOST_TEST( cli.parse_raw( ":create.task P 2" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":create.task P 2" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.task p_2" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.project p_2" ).result == fail );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.open.inactive.p_1.tasks.open.p_2" );
    // attempt to close p_1 with p_2 open
    BOOST_TEST( cli.parse_raw( ":close.project" ).result == fail );
    // create p_3
    BOOST_TEST( cli.parse_raw( ":create.project p_3" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":create.project p_3" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.project P 3" ).result == fail );
    BOOST_TEST( kmap.fetch_heading( kmap.selected_node() ).value() == "p_3" );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.open.inactive.p_3" );
    // add p_3 as a task of p_1
    BOOST_TEST( cli.parse_raw( ":select.node .root.projects.open.inactive.p_1" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":add.task p_3" ).result == succ );
    // close p_2
    BOOST_TEST( cli.parse_raw( ":select.node .root.projects.open.inactive.p_1.tasks.open.p_2" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":resolve.alias" ).result == succ );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.open.inactive.p_2" );
    BOOST_TEST( cli.parse_raw( ":close.project" ).result == succ );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.closed.p_2" );
    BOOST_TEST( kmap.exists( "/projects.closed.p_2" ) );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.p_1.tasks.closed.p_2" ) );
    // open p_2
    BOOST_TEST( cli.parse_raw( ":open-project" ).result == succ );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.open.inactive.p_2" );
    BOOST_TEST( kmap.exists( "/projects.open.inactive.p_1.tasks.open.p_2" ) );
    // activate p_2
    BOOST_TEST( cli.parse_raw( ":activate-project" ).result == succ );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.open.active.p_2" );
    // deactivate p_2
    BOOST_TEST( cli.parse_raw( ":deactivate-project" ).result == succ );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/projects.open.inactive.p_2" );
}
#endif // 0

BOOST_AUTO_TEST_SUITE_END( /* project */)
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test