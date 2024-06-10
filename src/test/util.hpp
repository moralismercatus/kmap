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
#define REQUIRE_RFAIL( ... ) REQUIRE( kmap::test::fail( __VA_ARGS__ ) )

#include <com/canvas/canvas.hpp>
#include <com/network/network.hpp>

#include <boost/filesystem/path.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <vector>
#include <string>

namespace kmap::com {
    class Database;
} // namespace kmap::com

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

struct ComponentFixture
{
	std::string file;
	uint32_t line;

	ComponentFixture( std::set< std::string > const& components
                    , std::string const& curr_file = __builtin_FILE()
	                , uint32_t const curr_line = __builtin_LINE() );
    ~ComponentFixture();
};

struct SaveToDiskFixture
{
    boost::filesystem::path file_path;
	com::Database& db;
	std::string file;
	uint32_t line;

	SaveToDiskFixture( com::Database& d
	                 , std::string const& curr_file 
	                 , uint32_t const curr_line );
	~SaveToDiskFixture();
};

struct DisableDebounceFixture
{
	std::string file;
	uint32_t line;
    bool prev_debounce = {};

    DisableDebounceFixture( std::string const& curr_file = __builtin_FILE()
                          , uint32_t const curr_line = __builtin_LINE() );
    ~DisableDebounceFixture();
};

template< typename Range >
auto headings( com::Network const& nw 
             , Range const& rng )
    -> std::vector< std::string >
{
    KM_RESULT_PROLOG();

    return rng
         | ranges::views::transform( [ & ]( auto const& e ){ return KTRYE( nw.fetch_heading( e ) ); } )
         | ranges::to< std::vector >();
}

} // namespace kmap::test

#define KMAP_COMPONENT_FIXTURE_SCOPED( ... ) kmap::test::ComponentFixture component_fixture{ { __VA_ARGS__ } }
#define KMAP_INIT_DISK_DB_FIXTURE_SCOPED( db ) kmap::test::SaveToDiskFixture save_to_disk_fixture{ ( db ), __FILE__, __LINE__ }
#define KMAP_DISABLE_DEBOUNCE_FIXTURE_SCOPED() kmap::test::DisableDebounceFixture disable_debounce_fixture{ __FILE__, __LINE__ }

#endif // KMAP_TEST_UTIL_HPP