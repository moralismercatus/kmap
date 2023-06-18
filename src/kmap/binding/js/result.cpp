/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <kmap/binding/js/result.hpp>

#include <common.hpp>
#include <error/master.hpp> // Payload
#include <js_iface.hpp>
#include <util/result.hpp>

#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>

#include <cstdint>
#include <vector>
#include <set>

namespace {

KMAP_BIND_RESULT( kmap::Uuid );
KMAP_BIND_RESULT( double );
KMAP_BIND_RESULT( float );
KMAP_BIND_RESULT( std::set<kmap::Uuid> );
KMAP_BIND_RESULT( std::set<uint16_t> );
KMAP_BIND_RESULT( std::set<uint32_t> );
KMAP_BIND_RESULT( std::set<uint64_t> );
KMAP_BIND_RESULT( std::set<uint8_t> );
KMAP_BIND_RESULT( std::string );
KMAP_BIND_RESULT( std::vector<kmap::Uuid> );
KMAP_BIND_RESULT( std::vector<uint16_t> );
KMAP_BIND_RESULT( std::vector<uint32_t> );
KMAP_BIND_RESULT( std::vector<uint64_t> );
KMAP_BIND_RESULT( std::vector<uint8_t> );
KMAP_BIND_RESULT( uint16_t );
KMAP_BIND_RESULT( uint32_t );
KMAP_BIND_RESULT( uint64_t );
KMAP_BIND_RESULT( uint8_t );

namespace void_t
{
    auto has_error( kmap::Result< void > const& rt ) -> bool { return rt.has_error(); }
    auto has_exception( kmap::Result< void > const& rt ) -> bool { return rt.has_exception(); }
    auto has_failure( kmap::Result< void > const& rt ) -> bool { return rt.has_failure(); }
    auto has_lost_consistency( kmap::Result< void > const& rt ) -> bool { return rt.has_lost_consistency(); }
    auto has_value( kmap::Result< void > const& rt ) -> bool { return rt.has_value(); }
    auto error( kmap::Result< void > const& rt ) -> kmap::error_code::Payload { return rt.error(); }
    auto error_message( kmap::Result< void > const& rt ) -> std::string { return to_string( rt.error() ); }
    auto throw_on_error( kmap::Result< void > const& rt ) -> void { if( rt.has_error() ) { KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( rt.error() ) ); } }
}

EMSCRIPTEN_BINDINGS( kmap_binding_result_void )
{
    emscripten::class_< kmap::Result< void > >( "Result<void>" )
        .function( "has_error", &void_t::has_error )
        .function( "has_exception", &void_t::has_exception )
        .function( "has_failure", &void_t::has_failure )
        .function( "has_lost_consistency", &void_t::has_lost_consistency )
        .function( "has_value", &void_t::has_value )
        .function( "error", &void_t::error ) 
        .function( "error_message", &void_t::error_message ) 
        .function( "throw_on_error", &void_t::throw_on_error ) 
        ;
}

// SCENARIO( "kmap::Result<void> JS bindings", "[js][js_iface]" )
// {
// }

// SCENARIO( "kmap::Result<T> JS bindings", "[js][js_iface]" )
// {
//     REQUIRE( kmap::js::eval< bool >( "kmap.failure( 'test' ).has_error()" ) );
// }

} // namespace anon
