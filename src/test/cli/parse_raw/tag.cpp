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
BOOST_AUTO_TEST_SUITE( tag
                     ,
                     * utf::disabled() ) // TODO: Re-enable test suite. Temporary disabled until reassessment of feature.
/******************************************************************************/
BOOST_AUTO_TEST_CASE( misc 
                    , 
                    * utf::depends_on( "kmap_iface" ) 
                    * utf::depends_on( "network" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    create_lineages( "1"
                   , "2" );

    // create tags.t_1
    BOOST_TEST( cli.parse_raw( ":create.tag t_1" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":create.tag t_1" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":create.tag: T 1" ).result == fail );
    BOOST_TEST( kmap.fetch_heading( kmap.selected_node() ).value() == "t_1" );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/tags.t_1" );
    // create tags.indirection.t_1
    BOOST_TEST( cli.parse_raw( ":create.tag indirection.t_1" ).result == succ );
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/tags.indirection.t_1" );
    // make t_1.indirection a tag of .root.a
    BOOST_TEST( cli.parse_raw( ":select.node .root.1" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":add.tag t_1.indirection" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":add.tag t_1.indirection" ).result == fail );
    BOOST_TEST( cli.parse_raw( ":add.tag t_1" ).result == fail ); // An alias of heading "t_1" already exists.
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/1" );
    // make t_1 a tag of .root.b
    BOOST_TEST( cli.parse_raw( ":select.node .root.2" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":add.tag t_1" ).result == succ );
    BOOST_TEST( cli.parse_raw( ":add.tag t_1" ).result == fail ); // Duplicate alias.
    BOOST_TEST( cli.parse_raw( ":add.tag t_1.indirection" ).result == fail ); // An alias of heading "t_1" already exists.
    BOOST_TEST( kmap.absolute_path_flat( kmap.selected_node() ) == "/2" );
}

BOOST_AUTO_TEST_SUITE_END( /* tag */ )
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test