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

#include <emscripten.h>
#include <emscripten/val.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kmap::js
{

auto lint( std::string const& code )
    -> Result< void >;

auto publish_function( std::string_view const name
                     , std::vector< std::string > const& params
                     , std::string_view const body )
    -> Result< void >;

template< typename Return
        , typename... Args >
auto call( emscripten::val& object 
         , std::string const& name
         , Args&&... args )
    -> Result< Return >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Return ); 

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

    auto rv = KMAP_MAKE_RESULT( Return ); 
    auto const eval_str = io::format( "( function(){{ try{{ return {} }} catch( err ){{ console.log( err ); return undefined; }} }} )();"
                                    , expr );

    // io::print( "evaluating string: {}\n", eval_str );

    KMAP_ENSURE( rv, lint( eval_str ), error_code::js::lint_failed );

    if( auto const res = val::global().call< val >( "eval"
                                                  , eval_str )
      ; !res.isUndefined() && !res.isNull() )
    {
        rv = res.template as< Return >();
    }
    else
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::js::eval_failed, io::format( "failed to evaluate javascript expression: {}", expr ) );
    }

    return rv;
}

auto eval_void( std::string const& expr )
    -> Result< void >;

template< typename Return >
auto fetch_element_by_id( std::string const& id )
    -> Result< Return >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( Return ); 

    rv = eval< Return >( io::format( "document.getElementById( '{}' );", id ) );

    return rv;
}

} // namespace kmap::js

#endif // KMAP_JS_IFACE_HPP
