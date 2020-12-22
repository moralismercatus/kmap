/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_JS_IFACE_HPP
#define KMAP_JS_IFACE_HPP

#include "common.hpp"

#include <emscripten.h>
#include <emscripten/val.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kmap::js
{

template< typename Return
        , typename... Args >
auto call( std::string_view name
         , Args&&... args )
    -> Optional< Return >;
auto publish_function( std::string_view const name
                     , std::vector< std::string > const& params
                     , std::string_view const body )
    -> bool;

// -------------------------------------------
template< typename Return
        , typename... Args >
auto call( std::string const& name
         , Args&&... args )
    -> Optional< Return >
{
    using emscripten::val;

    auto rv = Optional< Return >{};

    if( auto const res = val::global().call< val >( name.c_str()
                                                  , std::forward< Args >( args )... )
      ;  res.template as< bool >() )
    {
        rv = res.template as< Return >();
    }

    return rv;
}

} // namespace kmap::js

#endif // KMAP_JS_IFACE_HPP
