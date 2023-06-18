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

auto append_child_element( std::string const& parent_doc_id
                         , std::string const& child_doc_id )
    -> Result< void >;
auto beautify( std::string const& code )
    -> std::string;
auto create_child_element( std::string const& parent_id
                         , std::string const& child_id
                         , std::string const& elem_type )
    -> Result< void >;
auto create_html_canvas( std::string const& id )
    -> Result< void >;
auto eval_val( std::string const& expr )
    -> Result< emscripten::val >;
auto eval_void( std::string const& expr )
    -> Result< void >;
auto erase_child_element( std::string const& doc_id )
    -> Result< void >;
auto fetch_parent_element_id( std::string const& doc_id )
    -> Result< std::string >;
auto lint( std::string const& code )
    -> Result< void >;
auto move_element( std::string const& src_doc_id
                 , std::string const& dst_doc_id )
    -> Result< void >;
auto preprocess( std::string const& code )
    -> Result< std::string >;
auto publish_function( std::string_view const name
                     , std::vector< std::string > const& params
                     , std::string_view const body )
    -> Result< void >;
auto set_global_kmap( Kmap& kmap )
    -> void;
auto set_tab_index( std::string const& elem_id
                  , unsigned const& index )
    -> Result< void >;

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

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "name", name );

    auto rv = KMAP_MAKE_RESULT( Return ); 
    auto global = val::global();

    rv = KTRY( call< Return >( global, name, std::forward< Args >( args )... ) );

    return rv;
}

template< typename RT >
auto eval( std::string const& expr )
    -> Result< RT >
{
    using emscripten::val;

    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( RT ); 
    auto eval_res = KTRY( eval_val( expr ) );

    // TODO: There's got to be a way to get the underlying bound type for `RT`, check instanceof, as in `eval_val`, and then call `as()`, to avoid the possible exception on type casting.
    rv = eval_res.template as< RT >();

    return rv;
}

template< typename Return >
auto fetch_computed_property_value( std::string const& elem_id
                                  , std::string const& property )
    -> Result< Return >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "elem_id", elem_id );
        KM_RESULT_PUSH( "property", property );

    auto rv = KMAP_MAKE_RESULT( Return ); 

    rv = KTRY( eval< Return >( fmt::format( "return window.getComputedStyle( document.getElementById( '{}' ), null ).getPropertyValue( '{}' );"
                                          , elem_id
                                          , property ) ) );

    return rv;
}

template< typename Return >
auto fetch_element_by_id( std::string const& id )
    -> Result< Return >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

    auto rv = KMAP_MAKE_RESULT( Return ); 

    rv = KTRY( eval< Return >( io::format( "return document.getElementById( '{}' );", id ) ) );

    return rv;
}

auto exists( Uuid const& id )
    -> bool;
auto element_exists( std::string const& doc_id )
    -> bool;
auto fetch_style_member( std::string const& elem_id )
    -> Result< emscripten::val >;
auto is_child_element( std::string const& parent_id
                     , std::string const& child_id )
    -> bool;
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
