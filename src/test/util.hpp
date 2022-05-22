/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TEST_UTIL_HPP
#define KMAP_TEST_UTIL_HPP

// Assumes REQUIRE halts flow on failure.
#define REQUIRE_TRY( ... ) \
    ({ \
        auto&& res = ( __VA_ARGS__ ); \
        REQUIRE( kmap::test::succ( res ) ); \
        res.value(); \
    })
#define REQUIRE_RES( ... ) REQUIRE( kmap::test::succ( __VA_ARGS__ ) )

#include "canvas.hpp"
#include "js_iface.hpp"

#include <boost/filesystem/path.hpp>

namespace kmap::test {

template< concepts::Boolean T >
auto succ( T const& t
         , const char* file = __builtin_FILE() /* TODO: replace with std::source_location */
         , unsigned line = __builtin_LINE() /* TODO: replace with std::source_location */ )
{
    if constexpr( requires{ t.error(); } )
    {
        if( !t )
        {
            fmt::print( stderr, "Expected Result 'success' at {}:{}:\n{}\n", file, line, to_string( t.error() ) );
        }
    }
    return static_cast< bool >( t );
}
template< concepts::Boolean T >
auto fail( T const& t )
{
    return !static_cast< bool >( t );
}

struct BlankStateFixture
{
	std::string file;
	uint32_t line;

	BlankStateFixture( std::string const& curr_file 
	                 , uint32_t const curr_line );
	~BlankStateFixture();
};

struct SaveToDiskFixture
{
    boost::filesystem::path file_path;
	Database& db;
	std::string file;
	uint32_t line;

	SaveToDiskFixture( Database& d
	                 , std::string const& curr_file 
	                 , uint32_t const curr_line );
	~SaveToDiskFixture();
};

} // namespace kmap::test

#define KMAP_BLANK_STATE_FIXTURE_SCOPED() kmap::test::BlankStateFixture blank_state_fixture{ __FILE__, __LINE__}
#define KMAP_INIT_DISK_DB_FIXTURE_SCOPED( db ) kmap::test::SaveToDiskFixture save_to_disk_fixture{ ( db ), __FILE__, __LINE__ }

#endif // KMAP_TEST_UTIL_HPP