/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "test/util.hpp"

#include "common.hpp"
#include "com/database/db.hpp"
#include "com/filesystem/filesystem.hpp"
#include "error/master.hpp"
#include "kmap.hpp"
#include "test/master.hpp"

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

namespace fs = boost::filesystem;

namespace kmap::test {

ComponentFixture::ComponentFixture( std::set< std::string > const& components
                                  , std::string const& curr_file
                                  , uint32_t const curr_line )
    : file{ curr_file }
    , line{ curr_line }
{
    try
    {
        KM_RESULT_PROLOG(); 

        auto& kmap = Singleton::instance();

        // Register all components, first.
        // kmap.init_component_store();
        KTRYE( register_all_components() );
        // Determine just those needed for the component.
        auto deps = std::set< std::string >();
        for( auto const& component : components )
        {
            auto const ds = kmap.component_store().all_uninit_dependents( component );

            deps.insert( ds.begin(), ds.end() );
            deps.emplace( component );
        }
        fmt::print( "[ {} ] deps: [ {} ]\n"
                  , components | ranges::views::join( ',' ) | ranges::to< std::string >()
                  , deps | ranges::views::join( ',' ) | ranges::to< std::string >() );
        // "component_store" is special, as it is never "registered", as the "root" component.
        deps.erase( "component_store" ); // TODO: Is this right? component_store isn't registered? What if it is?

#if KMAP_NATIVE && 0
        auto const registered_components = kmap.component_store().registered_components();
#endif // !KMAP_NATIVE

        // Reset store.
        KTRYE( kmap.clear_component_store() );
        // Register and initialize only needed components.
        kmap.init_component_store();
        KTRYE( register_components( deps ) );

#if KMAP_NATIVE && 0
        // TODO: Filter to just "deps"... Meh... should still all work, but this isn't filtering to just "deps".
        for( auto const& component : registered_components )
        {
            #if KMAP_LOG && 0
            fmt::print( "re-registering: {}\n", component.first );
            #endif
            KTRYE( kmap.component_store().register_component( component.second ) );
        }
#endif // !KMAP_NATIVE

        KTRYE( kmap.component_store().fire_initialized( "component_store" ) );
    }
    catch( std::exception const& e )
    {
        fmt::print( stderr, "Fixture ctor failed: {}|{}\n", file, line );
        std::cerr << e.what() << '\n';
        throw;
    }
}

ComponentFixture::~ComponentFixture()
{
    auto& kmap = Singleton::instance(); // TODO: Why kmap's ctor automatically creating a db.root_node? For proper testing, seems like this shouldn't be auto happening.

    try
    {
        KM_RESULT_PROLOG();
#if KMAP_NATIVE && 0
        auto const registered_components = kmap.component_store().registered_components();
#endif // !KMAP_NATIVE

        KTRYE( kmap.clear() );

#if KMAP_NATIVE && 0
        // TODO: Filter to just "deps"... Meh... should still all work, but this isn't filtering to just "deps".
        for( auto const& component : registered_components )
        {
            #if KMAP_LOG && 0
            fmt::print( "re-registering: {}\n", component.first );
            #endif
            KTRYE( kmap.component_store().register_component( component.second ) );
        }
#endif // !KMAP_NATIVE
    }
    catch( std::exception const& e )
    {
        fmt::print( stderr, "Fixture dtor failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

SaveToDiskFixture::SaveToDiskFixture( com::Database& d
                                    , std::string const& curr_file 
                                    , uint32_t const curr_line )
	: file_path{ com::kmap_root_dir / fmt::format( "test.{}.kmap", to_string( gen_uuid() ) ) }
	, db{ d }
	, file{ curr_file }
	, line{ curr_line }
{
    try
    {
        KM_RESULT_PROLOG();

        KTRYE( db.init_db_on_disk( file_path ) );
        KMAP_ENSURE_EXCEPT( db.has_file_on_disk() );
    }
    catch( std::exception const& e )
    {
        fmt::print( stderr, "Fixture ctor failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
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
        fmt::print( stderr, "Fixture dtor failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
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
        // TODO: These stopped working in Clang 16.
		// REQUIRE( 0 == any_of( 0, 1, 2 ) );
		// REQUIRE( 3 != any_of( 0, 1, 2 ) );
		// REQUIRE( 0 == none_of( 1, 2, 3 ) );
	}
}