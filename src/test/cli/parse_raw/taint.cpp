/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../master.hpp"
#include "../../../cmd/taint.hpp"

#include <boost/test/unit_test.hpp>

namespace utf = boost::unit_test;

namespace kmap::test {

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( cli )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( parse_raw )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( project )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( taint )
/******************************************************************************/

auto init_trace_project( Kmap& kmap
                       , std::string const& path )
    -> void
{
    auto& cli = kmap.cli();
    auto const succ = CliResultCode::success;

    BOOST_TEST( cli.parse_raw( ":create.taint.project 1" ).result == succ );

    BOOST_REQUIRE( cmd::is_taint_project( kmap, kmap.selected_node() ) );

    auto const nodes = kmap.node_view( kmap.selected_node() );

    kmap.update_body( nodes[ "resources.ip_trace_file" ], io::format( "{}/ip_trace.bin", path ) );
    kmap.update_body( nodes[ "resources.backlinks_file" ], io::format( "{}/backlinks.bin" ) );
    kmap.update_body( nodes[ "resources.backlink_map" ], io::format( "{}/backlink_map.bin" ) );
    kmap.update_body( nodes[ "resources.memory_origins" ], io::format( "{}/memory_origins.bin" ) );
}

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( trace_1 )
/******************************************************************************/

BOOST_AUTO_TEST_CASE( /*taint*/select_origins_from_indices
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    init_trace_project( kmap, "src/test/cli/parse_raw/data/trace_1" );

    auto const nodes = kmap.node_view( kmap.selected_node() );

    BOOST_TEST( cli.parse_raw( ":select.origins.from.indices 0 1" ).result == succ );
    BOOST_TEST( "[0x8049098,0x804909b]" == kmap.fetch_body( nodes[ "result.origin.index.0_1" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.origins.from.indices 1 2" ).result == succ );
    BOOST_TEST( "[0x8049098,0x804909b]" == kmap.fetch_body( nodes[ "result.origin.index.1_2" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.origins.from.indices 2 3" ).result == succ );
    BOOST_TEST( "[0x8049098,0x804909b]" == kmap.fetch_body( nodes[ "result.origin.index.2_3" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.origins.from.indices 3 4" ).result == succ );
    BOOST_TEST( "[0x8049098,0x804909b]" == kmap.fetch_body( nodes[ "result.origin.index.3_4" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.origins.from.indices 4 5" ).result == succ );
    BOOST_TEST( "" == kmap.fetch_body( nodes[ "result.origin.index.4_5" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.origins.from.indices 5 6" ).result == succ );
    BOOST_TEST( "" == kmap.fetch_body( nodes[ "result.origin.index.5_6" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.origins.from.indices 6 7" ).result == succ );
    BOOST_TEST( "" == kmap.fetch_body( nodes[ "result.origin.index.6_7" ] ).value() );
}

BOOST_AUTO_TEST_CASE( /*trace_1*/select_addresses_from_origins
                    , 
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    init_trace_project( kmap, "src/test/cli/parse_raw/data/trace_1" );

    auto const nodes = kmap.node_view( kmap.selected_node() );

    BOOST_TEST( cli.parse_raw( ":select.addresses.from.origins 0x8049098 0x8049099" ).result == succ );
    BOOST_TEST( "[0x8048080]<br>[0x8048085]<br>[0x8048087]<br>[0x804808c]" == kmap.fetch_body( nodes[ "result.origin.index.0x8049098_0x8049099" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.addresses.from.origins 0x8049099 0x804909a" ).result == succ );
    BOOST_TEST( "[0x8048080]<br>[0x8048085]<br>[0x8048087]<br>[0x804808c]" == kmap.fetch_body( nodes[ "result.origin.index.0x8049099_0x804909a" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.addresses.from.origins 0x804909a 0x804909b" ).result == succ );
    BOOST_TEST( "[0x8048080]<br>[0x8048085]<br>[0x8048087]<br>[0x804808c]" == kmap.fetch_body( nodes[ "result.origin.index.0x804909a_0x804909b" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.addresses.from.origins 0x804909b 0x804909c" ).result == succ );
    BOOST_TEST( "[0x8048080]<br>[0x8048085]<br>[0x8048087]<br>[0x804808c]" == kmap.fetch_body( nodes[ "result.origin.index.0x804909b_0x804909c" ] ).value() );

    BOOST_TEST( cli.parse_raw( ":select.addresses.from.origins 0x804909c 0x804909d" ).result == succ );
    BOOST_TEST( "" == kmap.fetch_body( nodes[ "result.origin.index.0x804909c_0x804909d" ] ).value() );
}

BOOST_AUTO_TEST_SUITE_END( /* trace_1 */ )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( trace_2 )
/******************************************************************************/

BOOST_AUTO_TEST_CASE( /*trace_2*/select_unauthenticated_code
                    ,  // TODO: depends on parse_raw/create_command!
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();
    auto const succ = CliResultCode::success;
    auto const fail = CliResultCode::failure;

    {
        auto const guard =
        R"(
        if( kmap.is_in_taint_project( kmap.resolve_alias( kmap.selected_node() ).value() ) )
        {
            return kmap.success( "taint project" );
        }
        else
        {
            return kmap.failure( "selected node not in a taint project" );
        }
        )";
        auto const action =
        R"(
        const trace = kmap.make_trace();
        const begin = args.get( 0 );
        const end = args.get( 1 );
        const result_path = `unauth_code.${begin}_${end}`;

        trace.clean_result( result_path );

        const auth_region = trace.memory_region( begin, end );

        if( code_region.has_error() ) return kmap.failure( code_region.error_message() );

        const auth_indices = trace.select_indices_from_addresses( auth_region.value() );
        const code_region = trace.memory_region( '0x8048080', '0x8048095' );

        if( code_region.has_error() ) return kmap.failure( code_region.error_message() );

        const auth_origins = trace.select_origins_from_indices( auth_indices.value() );
        const unauth_code = trace.set_difference( code_region.value(), auth_origins.value() );
        const result = trace.print_result_set( result_path, unauth_code );

        kmap.select_node( kmap.uuid_to_string( result.value() ).value() );

        return kmap.success( 'query complete' );
        )";

        BOOST_REQUIRE( cli.parse_raw( ":create.command select.unauth_code.from.address" ) );

        auto const nodes = kmap.node_view( kmap.selected_node() );

        kmap.update_title( nodes[ "unconditional"], "taint_project" );

        kmap.update_body( nodes[ "taint_project.guard" ], guard );
        kmap.update_body( nodes[ "taint_project.action" ], action );
    }

    init_trace_project( kmap, "src/test/cli/parse_raw/data/trace_2" );

    auto const nodes = kmap.node_view( kmap.selected_node() );

    BOOST_TEST( cli.parse_raw( ":select.unauth_code.from.address 0x80480a0 0x80480a1" ).result == succ );
    BOOST_TEST( "[0x8048088,0x8048094]" == kmap.fetch_body( nodes[ "result.unauth_code.address.0x80480a0_0x80480a1" ] ).value() );
}

BOOST_AUTO_TEST_SUITE_END( /* trace_2 */ )

BOOST_AUTO_TEST_SUITE_END( /* taint */ )
BOOST_AUTO_TEST_SUITE_END( /* project */)
BOOST_AUTO_TEST_SUITE_END( /* parse_raw */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test