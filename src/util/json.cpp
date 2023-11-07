/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <util/json.hpp>

#include <util/result.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#include <emscripten/bind.h>
#endif // !KMAP_NATIVE

#include <boost/json.hpp>
#include <catch2/catch_test_macros.hpp>


#include <string>

namespace bjn = boost::json;

namespace kmap {

auto fetch_array( bjn::object const& obj
                , std::string const& key )
    -> Result< bjn::array >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "key", key );

    auto const at = KTRY( fetch_value( obj, key ) );

    KMAP_ENSURE( at.is_array(), error_code::common::uncategorized );

    return at.as_array();
}

auto fetch_bool( bjn::object const& obj
               , std::string const& key )
    -> Result< bool >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "key", key );

    auto const at = KTRY( fetch_value( obj, key ) );

    KMAP_ENSURE( at.is_bool(), error_code::common::uncategorized );

    return at.as_bool();
}

auto fetch_float( bjn::object const& obj
                , std::string const& key )
    -> Result< double >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "key", key );

    auto const at = KTRY( fetch_value( obj, key ) );

    KMAP_ENSURE( at.is_number(), error_code::common::uncategorized );

    return at.to_number< double >(); // Use to_number to avoid exact conversion failure case for when v is e.g., 0.0, and is converted to integer type automatically.
}

auto fetch_string( bjn::object const& obj
                 , std::string const& key )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "key", key );

    auto const at = KTRY( fetch_value( obj, key ) );

    KMAP_ENSURE( at.is_string(), error_code::common::uncategorized );

    return std::string{ at.as_string() };
}

auto fetch_value( bjn::object const& obj
                , std::string const& key )
    -> Result< bjn::value >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "key", key );

    KMAP_ENSURE( obj.contains( key ), error_code::common::uncategorized );

    return obj.at( key );
}

} // namespace kmap

SCENARIO( "json::object", "[json]" )
{
}

namespace kmap::binding::json {

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

#if !KMAP_NATIVE
using namespace emscripten;

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
#endif // !KMAP_NATIVE

} // namespace kmap::binding::json
