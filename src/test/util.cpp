/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "test/util.hpp"

#include "common.hpp"
#include "db.hpp"
#include "error/master.hpp"
#include "kmap.hpp"
#include "test/master.hpp"

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>

namespace fs = boost::filesystem;

namespace kmap::test {

BlankStateFixture::BlankStateFixture( std::string const& curr_file
									, uint32_t const curr_line )
	: file{ curr_file }
	, line{ curr_line }
{
    try
    {
        auto& kmap = Singleton::instance(); // TODO: Why kmap's ctor automatically creating a db.root_node? For proper testing, seems like this shouldn't be auto happening.

        KMAP_TRYE( js::create_html_canvas( to_string( network_uuid ) ) ); // Must set up a dummy canvas for a dummy 'network' without kmap::Canvas involvement.
        KMAP_TRYE( kmap.reset_database() );
        KMAP_TRYE( kmap.set_up_db_root() );
        KMAP_TRYE( kmap.reset_network() );
        KMAP_TRYE( kmap.set_up_nw_root() );
    }
    catch( std::exception const& e )
    {
        fmt::print( stderr, "Fixture failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
        std::cerr << e.what() << '\n';
        throw;
    }
}

BlankStateFixture::~BlankStateFixture()
{
    auto& kmap = Singleton::instance(); // TODO: Why kmap's ctor automatically creating a db.root_node? For proper testing, seems like this shouldn't be auto happening.

    try
    {
        // Can't call `kmap.erase_node( root )` b/c it's API enforces that root node is not erased. Must go round about.
        for( auto const& child : kmap.fetch_children( kmap.root_node_id() ) )
        {
            KMAP_TRYE( kmap.erase_node( child ) );
        }
        KMAP_TRYE( kmap.network().remove( kmap.root_node_id() ) );
        kmap.database().erase_all( kmap.root_node_id() );
        KMAP_TRYE( js::erase_child_element( to_string( network_uuid ) ) );
    }
    catch( std::exception const& e )
    {
        fmt::print( stderr, "Fixture failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

SaveToDiskFixture::SaveToDiskFixture( Database& d
                                    , std::string const& curr_file 
                                    , uint32_t const curr_line )
	: file_path{ kmap_root_dir / fmt::format( "test.{}.kmap", to_string( gen_uuid() ) ) }
	, db{ d }
	, file{ curr_file }
	, line{ curr_line }
{
    try
    {
        KMAP_TRYE( db.init_db_on_disk( file_path ) );
        KMAP_ENSURE_EXCEPT( db.has_file_on_disk() );
    }
    catch( std::exception const& e )
    {
        fmt::print( stderr, "Fixture failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
        std::cerr << e.what() << '\n';
        throw;
    }
}

SaveToDiskFixture::~SaveToDiskFixture()
{
    try
    {
        KMAP_ENSURE_EXCEPT( fs::remove( file_path ) );
    }
    catch( std::exception const& e )
    {
        fmt::print( stderr, "Fixture failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}
 
} // namespace kmap::test

SCENARIO( "general utility tests" )
{
	using namespace kmap::util;
	GIVEN( "nothing" )
	{
		REQUIRE( 0 == any_of( 0, 1, 2 ) );
		REQUIRE( 3 != any_of( 0, 1, 2 ) );
		REQUIRE( 0 == none_of( 1, 2, 3 ) );
	}
}