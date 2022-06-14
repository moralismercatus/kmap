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

template< typename EntryFn
        , typename ExitFn >
struct Fixture
{
    EntryFn entry_fn;
    ExitFn exit_fn;
    std::string file;
    uint32_t line;

    Fixture( EntryFn entry
           , ExitFn exit
           , std::string const &curr_file = __builtin_FILE()
           , uint32_t const curr_line = __builtin_LINE() )
        : entry_fn{ entry }
        , exit_fn{ exit }
        , file{ curr_file }
        , line{ curr_line }
    {
        try
        {
            entry_fn();
        }
        catch( std::exception const& e )
        {
            fmt::print( stderr, "Fixture ctor failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
            std::cerr << e.what() << '\n';
            throw;
        }
    }
    ~Fixture()
    {
        try
        {
            exit_fn();
        }
        catch( std::exception const& e )
        {
            fmt::print( stderr, "Fixture dtor failed: {}|{}\n", file, line ); // Can't throw exception in dtor.
            std::cerr << e.what() << '\n';
            std::terminate();
        }
    }
};

struct BlankStateFixture
{
    std::string file;
    uint32_t line;

    BlankStateFixture( std::string const& curr_file 
                     , uint32_t const curr_line );
    ~BlankStateFixture();
};

struct CommandFixture
{
	std::string file;
	uint32_t line;

	CommandFixture( std::string const& curr_file 
	              , uint32_t const curr_line );
    ~CommandFixture();
};

struct DatabaseRootFixture
{
	std::string file;
	uint32_t line;

	DatabaseRootFixture( std::string const &curr_file
	                   , uint32_t const curr_line );
	~DatabaseRootFixture();
};

struct EventFixture
{
	std::string file;
	uint32_t line;

	EventFixture( std::string const& curr_file 
	            , uint32_t const curr_line );
	~EventFixture();
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

#define KMAP_BLANK_STATE_FIXTURE_SCOPED() kmap::test::BlankStateFixture blank_state_fixture{ __FILE__, __LINE__ }
#define KMAP_DATABASE_ROOT_FIXTURE_SCOPED() kmap::test::DatabaseRootFixture database_root_fixture{ __FILE__, __LINE__ }
#define KMAP_COMMAND_FIXTURE_SCOPED() kmap::test::CommandFixture command_fixture{ __FILE__, __LINE__ }
#define KMAP_EVENT_FIXTURE_SCOPED() kmap::test::EventFixture event_fixture{ __FILE__, __LINE__ }
#define KMAP_INIT_DISK_DB_FIXTURE_SCOPED( db ) kmap::test::SaveToDiskFixture save_to_disk_fixture{ ( db ), __FILE__, __LINE__ }

#endif // KMAP_TEST_UTIL_HPP