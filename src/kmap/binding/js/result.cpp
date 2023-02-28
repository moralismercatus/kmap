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

auto error_message_void( kmap::Result< void > const& rt )
    -> std::string
{
    return to_string( rt.error() );
}

EMSCRIPTEN_BINDINGS( kmap_binding_result_void )
{
    emscripten::class_< kmap::Result< void > >( "Result2<void>" )
        .function( "has_error", &kmap::Result< void >::has_error ) \
        .function( "has_exception", &kmap::Result< void >::has_exception ) \
        .function( "has_failure", &kmap::Result< void >::has_failure ) \
        .function( "has_lost_consistency", &kmap::Result< void >::has_lost_consistency ) \
        .function( "has_value", &kmap::Result< void >::has_value ) \
        .function( "error", &error_message_void ) 
        ;
}

} // namespace anon
