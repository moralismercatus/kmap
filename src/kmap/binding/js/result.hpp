/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_BINDING_JS_RESULT_HPP
#define KMAP_BINDING_JS_RESULT_HPP

#include "common.hpp"
#include "util/macro.hpp"
#include <kmap/binding/js/util.hpp>

#include <emscripten/bind.h>

// TODO: Remove "2" suffix. Temporary until binding::Result is fully replaced.
#define KMAP_BIND_RESULT2_AUX( nspace, type ) \
    namespace nspace{ namespace { \
        auto error_message( kmap::Result< type > const& rt ) \
            -> std::string \
        { \
            return to_string( rt.error() ); \
        } \
        KMAP_EMSCRIPTEN_BINDINGS( KMAP_UNIQUE_NAME( kmap_bind_ ) ) \
        { \
            emscripten::class_< kmap::Result< type > >( "Result2<"#type">" ) \
                .function( "has_error", &kmap::Result< type >::has_error ) \
                .function( "has_exception", &kmap::Result< type >::has_exception ) \
                .function( "has_failure", &kmap::Result< type >::has_failure ) \
                .function( "has_lost_consistency", &kmap::Result< type >::has_lost_consistency ) \
                .function( "has_value", &kmap::Result< type >::has_value ) \
                .function( "error", &error_message ) \
                ; \
        } \
    } }
#define KMAP_BIND_RESULT2( type ) KMAP_BIND_RESULT2_AUX( KMAP_UNIQUE_NAME( binding_ ), type )
   
#endif // KMAP_BINDING_JS_RESULT_HPP
