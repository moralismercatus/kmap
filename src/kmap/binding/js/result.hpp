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

// TODO: I believe functions marked with "noexcept" i.e., Result::has_value() etc, get ignored by embind. Curious, as it's a silent failure.
//       I believe that `https://github.com/emscripten-core/emscripten/pull/15273` fixes this (to test), but the likes of `Result::has_value()` is also constexpr,
//       so I don't know if that would cause a silent failure either.
//       I'm providing wrappers for the Result< T >::has_* functions to avoid their being silently missed for now, but, in theory, these bindings can go directly to Result< T >
//       in the future.
#define KMAP_BIND_RESULT_AUX( nspace, type ) \
    namespace nspace{ namespace { \
        auto error( kmap::Result< type > const& rt ) \
            -> kmap::error_code::Payload \
        { \
            return rt.error(); \
        } \
        auto error_message( kmap::Result< type > const& rt ) \
            -> std::string \
        { \
            return to_string( rt.error() ); \
        } \
        auto has_error( kmap::Result< type > const& rt ) \
            -> bool \
        { \
            return rt.has_error(); \
        } \
        auto has_exception( kmap::Result< type > const& rt ) \
            -> bool \
        { \
            return rt.has_exception(); \
        } \
        auto has_failure( kmap::Result< type > const& rt ) \
            -> bool \
        { \
            return rt.has_failure(); \
        } \
        auto has_lost_consistency( kmap::Result< type > const& rt ) \
            -> bool \
        { \
            return rt.has_lost_consistency(); \
        } \
        auto has_value( kmap::Result< type > const& rt ) \
            -> bool \
        { \
            return rt.has_value(); \
        } \
        auto throw_on_error( kmap::Result< type >const& rt ) \
            -> void \
        { \
            if( rt.has_error() ) \
            { \
                KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( rt.error() ) ); \
            } \
        } \
        auto value( kmap::Result< type > const& rt ) \
            -> type const& \
        { \
            return rt.value(); \
        } \
        KMAP_EMSCRIPTEN_BINDINGS( KMAP_UNIQUE_NAME( kmap_bind_ ) ) \
        { \
            emscripten::class_< kmap::Result< type > >( "Result<"#type">" ) \
                .function( "error", &error ) \
                .function( "error_message", &error_message ) \
                .function( "has_error", &has_error ) \
                .function( "has_exception", &has_exception ) \
                .function( "has_failure", &has_failure ) \
                .function( "has_lost_consistency", &has_lost_consistency ) \
                .function( "has_value", &has_value ) \
                .function( "throw_on_error", &throw_on_error ) \
                .function( "value", &value ) \
                ; \
        } \
    } }
#define KMAP_BIND_RESULT( type ) KMAP_BIND_RESULT_AUX( KMAP_UNIQUE_NAME( binding_ ), type )
   
#endif // KMAP_BINDING_JS_RESULT_HPP
