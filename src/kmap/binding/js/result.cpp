/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <kmap/binding/js/result.hpp>

#include "common.hpp"
#include "error/master.hpp" // Payload
#include "util/result.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>

#include <cstdint>
#include <vector>
#include <set>

namespace {

KMAP_BIND_RESULT2( kmap::Uuid );
KMAP_BIND_RESULT2( double );
KMAP_BIND_RESULT2( float );
KMAP_BIND_RESULT2( std::set<kmap::Uuid> );
KMAP_BIND_RESULT2( std::set<uint16_t> );
KMAP_BIND_RESULT2( std::set<uint32_t> );
KMAP_BIND_RESULT2( std::set<uint64_t> );
KMAP_BIND_RESULT2( std::set<uint8_t> );
KMAP_BIND_RESULT2( std::string );
KMAP_BIND_RESULT2( std::vector<kmap::Uuid> );
KMAP_BIND_RESULT2( std::vector<uint16_t> );
KMAP_BIND_RESULT2( std::vector<uint32_t> );
KMAP_BIND_RESULT2( std::vector<uint64_t> );
KMAP_BIND_RESULT2( std::vector<uint8_t> );
KMAP_BIND_RESULT2( uint16_t );
KMAP_BIND_RESULT2( uint32_t );
KMAP_BIND_RESULT2( uint64_t );
KMAP_BIND_RESULT2( uint8_t );

namespace void_t
{
    auto has_error( kmap::Result< void > const& rt ) -> bool { return rt.has_error(); }
    auto has_exception( kmap::Result< void > const& rt ) -> bool { return rt.has_exception(); }
    auto has_failure( kmap::Result< void > const& rt ) -> bool { return rt.has_failure(); }
    auto has_lost_consistency( kmap::Result< void > const& rt ) -> bool { return rt.has_lost_consistency(); }
    auto has_value( kmap::Result< void > const& rt ) -> bool { return rt.has_value(); }
    auto error( kmap::Result< void > const& rt ) -> std::string { return to_string( rt.error() ); }
}

EMSCRIPTEN_BINDINGS( kmap_binding_result_void )
{
    emscripten::class_< kmap::Result< void > >( "Result2<void>" )
        .function( "has_error", &void_t::has_error )
        .function( "has_exception", &void_t::has_exception )
        .function( "has_failure", &void_t::has_failure )
        .function( "has_lost_consistency", &void_t::has_lost_consistency )
        .function( "has_value", &void_t::has_value )
        .function( "error", &void_t::error ) 
        ;
}

} // namespace anon
