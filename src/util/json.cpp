/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <js_iface.hpp>

#include <boost/json.hpp>
#include <catch2/catch_test_macros.hpp>
#include <emscripten/bind.h>

#include <string>

SCENARIO( "json::object", "[json]" )
{
}

namespace kmap::binding::json {

using namespace emscripten;

auto at( boost::json::object const& jo
       , std::string const& key )
    -> boost::json::value const&
{
    return jo.at( key );
}

auto as_string( boost::json::value const& jv )
    -> std::string
{
    return std::string{ jv.as_string() };
}

auto to_string( boost::json::value const& jv )
    -> std::string
{
         if( jv.is_array() ) { KMAP_THROW_EXCEPTION_MSG( "TODO: Impl." ); }
    else if( jv.is_bool() ) { return jv.as_bool() ? "true" : "false"; }
    else if( jv.is_double() ) { return std::to_string( jv.as_double() ); }
    else if( jv.is_int64() ) { return std::to_string( jv.as_int64() ); }
    else if( jv.is_null() ) { KMAP_THROW_EXCEPTION_MSG( "value expected: got NULL" ); }
    else if( jv.is_object() ) { KMAP_THROW_EXCEPTION_MSG( "TODO: Impl." ); }
    else if( jv.is_string() ) { return std::string{ jv.as_string() }; }
    else if( jv.is_uint64() ) { return std::to_string( jv.as_uint64() ); }
    else
    {
        assert( false && "shouldn't be possible to reach this point" );
        KMAP_THROW_EXCEPTION_MSG( "Unreachable reached!" );
    }
}

EMSCRIPTEN_BINDINGS( kmap_json )
{
    class_< boost::json::value >( "boost::json::value" )
        .function( "as_string", &kmap::binding::json::as_string )
        .function( "to_string", &kmap::binding::json::to_string )
        ;
    class_< boost::json::object >( "boost::json::object" )
        .function( "at", &kmap::binding::json::at )
        ;
    class_< boost::json::string_view >( "boost::json::string_view" )
        ;
}

} // namespace kmap::binding::json
