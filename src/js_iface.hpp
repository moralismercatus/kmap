/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_JS_IFACE_HPP
#define KMAP_JS_IFACE_HPP

#include "common.hpp"
#include "contract.hpp"
#include "error/js_iface.hpp"
#include "error/master.hpp"
#include "utility.hpp"
#include "util/result.hpp"

#include <emscripten.h>
#include <emscripten/val.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kmap::js {

auto beautify( std::string const& code )
    -> std::string;
auto create_html_canvas( std::string const& id )
    -> Result< void >;
auto erase_child_element( std::string const& doc_id )
    -> Result< void >;
auto lint( std::string const& code )
    -> Result< void >;
auto publish_function( std::string_view const name
                     , std::vector< std::string > const& params
                     , std::string_view const body )
    -> Result< void >;
auto set_global_kmap( Kmap& kmap )
    -> void;

template< typename Return
        , typename... Args >
auto call( emscripten::val& object 
         , std::string const& name
         , Args&&... args )
    -> Result< Return >
{
    using emscripten::val;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "name", name );

    auto rv = result::make_result< Return >(); 

    if( auto const res = object.call< val >( name.c_str()
                                           , std::forward< Args >( args )... )
      ; !res.isUndefined() && !res.isNull() )
    {
        rv = res.template as< Return >();
    }

    return rv;
}

template< typename Return
        , typename... Args >
auto call( std::string const& name
         , Args&&... args )
    -> Result< Return >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Return ); 
    auto global = val::global();

    rv = call< Return >( global, name, std::forward< Args >( args )... );

    return rv;
}

template< typename Return >
auto eval( std::string const& expr )
    -> Result< Return >
{
    using emscripten::val;

    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Return ); 
    auto constexpr script =
R"%%%(
(
    function()
    {{
        try
        {{
            {}
        }}
        catch( err )
        {{
            if( is_cpp_exception( err ) )
            {{
                if( kmap.is_signal_exception( err ) )
                {{
                    console.log( 'signal exception recieved' );
                    throw err;
                }}
                else
                {{
                    console.log( '[kmap][error] std::exception encountered:' );
                    // print std exception
                    kmap.print_std_exception( err );
                }}
            }}
            else // Javascript exception
            {{
                console.log( '[kmap][error] JS exception: ' + err );
            }}
            return undefined;
        }}
    }}
)();
)%%%";
    auto const eval_str = io::format( script, expr );

    KMAP_ENSURE( lint( eval_str ), error_code::js::lint_failed );

    auto const eval_res = val::global().call< val >( "eval", eval_str );

    if( !eval_res.isUndefined() && !eval_res.isNull() )
    {
        rv = eval_res.template as< Return >();
    }
    else
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::js::eval_failed, io::format( "Null returned (did you fail to return a result?) failed to evaluate javascript expression: {}", expr ) );
    }

    return rv;
}

auto eval_void( std::string const& expr )
    -> Result< void >;

template< typename Return >
auto fetch_computed_property_value( std::string const& elem_id
                                  , std::string const& property )
    -> Result< Return >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Return ); 

    rv = eval< Return >( fmt::format( "return window.getComputedStyle( document.getElementById( '{}' ), null ).getPropertyValue( '{}' );"
                                    , elem_id
                                    , property ) );

    return rv;
}

template< typename Return >
auto fetch_element_by_id( std::string const& id )
    -> Result< Return >
{
    auto rv = KMAP_MAKE_RESULT( Return ); 

    rv = eval< Return >( io::format( "return document.getElementById( '{}' );", id ) );

    return rv;
}

auto exists( Uuid const& id )
    -> bool;
auto element_exists( std::string const& doc_id )
    -> bool;
auto fetch_style_member( std::string const& elem_id )
    -> Result< emscripten::val >;
auto is_global_kmap_valid()
    -> bool;

class ScopedCode
{
    std::string ctor_code;
    std::string dtor_code;

public:
    ScopedCode() = default;
    ScopedCode( std::string const& ctor
              , std::string const& dtor );
    ScopedCode( ScopedCode&& other );
    ScopedCode( ScopedCode const& other ) = delete;
    ~ScopedCode();

    auto operator=( ScopedCode&& other ) -> ScopedCode&;
    auto operator=( ScopedCode const& other ) -> ScopedCode& = delete;
};

} // namespace kmap::js

#endif // KMAP_JS_IFACE_HPP
