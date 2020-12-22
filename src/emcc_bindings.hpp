/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EMCC_BINDINGS_HPP
#define KMAP_EMCC_BINDINGS_HPP

#include "common.hpp"
#include "io.hpp"

#include <string>

#define KMAP_BIND_RESULT( type ) \
    class_< kmap::binding::Result< type > >( "Result<"#type">" ) \
        .function( "error_message", &kmap::binding::Result< type >::error_message ) \
        .function( "has_error", &kmap::binding::Result< type >::has_error ) \
        .function( "has_value", &kmap::binding::Result< type >::has_value ) \
        .function( "value", &kmap::binding::Result< type >::value ) \
        ;
   

namespace kmap::binding {

/**
 * Extensible wrapper for communicating kmap::Result< T > to javascript.
 * Simply expose your type (kmap::Result< MyType >) to Emscripten's binding mechanism.
 */
template< typename T >
struct Result
{
    Result( kmap::Result< T >&& r )
        : result{ std::move( r ) }
    {
    }
    template< typename EC >
    Result( outcome::failure_type< EC, void > const& ft )
        : Result< T >{ ft.error() }
    {
    }

    auto error_message() const
        -> std::string
    {
        io::print( stderr
                 , "kmap::binding::Result< T >::error_message() called:\n{}\n"
                 , to_string( result.error() ) );

        return result.error().ec.message();
    }

    auto has_error() const
        -> bool
    {
        return result.has_error();
    }

    auto has_value() const
        -> bool
    {
        return result.has_value();
    }

    auto value()
        -> T&
    {
        return result.value();
    }

    auto success_message() const
        -> std::string
    {
        return success_message;
    }

    kmap::Result< T > result;
    Optional< std::string > failure_message = {}; // Optional payload for CLI output. Bit of a hack. Subject to future improvement as understanding of payloads in Boost.Outcome increases.
};

/**
 * Just like the others except missing value() mfunction.
 **/
template<>
struct Result< void >
{
    Result( kmap::Result< void >&& r )
        : result{ std::move( r ) }
    {
    }
    template< typename EC >
    Result( outcome::failure_type< EC, void > const& ft )
        : Result< void >{ ft.error() }
    {
    }

    auto error_message()
        -> std::string
    {
        return to_string( result.error() );
    }

    auto has_error()
        -> bool
    {
        return result.has_error();
    }

    auto has_value()
        -> bool
    {
        return result.has_value();
    }

    kmap::Result< void > result;
};

} // namespace kmap::binding

#endif // KMAP_EMCC_BINDINGS_HPP