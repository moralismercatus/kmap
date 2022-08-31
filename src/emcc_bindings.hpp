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
        .function( "throw_on_error", &kmap::binding::Result< type >::throw_on_error ) \
        .function( "value", &kmap::binding::Result< type >::value ) \
        .function( "value_or_throw", &kmap::binding::Result< type >::value_or_throw ) \
        ;
   

namespace kmap::binding {

// TODO: I suspect this is the wrong approach to binding. It requires, effectively, duplicating all desired structs/classes because of dependent types, some of which
//       are hidden under abstractions. Along with copious duplication, interplay/conversion between the binding::T and kmap::T can be difficult.
//       I think, rather, the way to achieve this is to bind the original type (e.g., kmap::Result< T >), and for specific routines, provide function utilities.
//       E.g. "kmap::Result< T >": to get the error message in JS, one also has to bind decltype( r.error() ), to finally get to r.error().message().
//       Rather, provide a utility class with functions as needed: `str = kmap.Result#T$.error_message( r );`
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

    auto success_message() const
        -> std::string
    {
        return "TODO: Success message";
    }

    auto throw_on_error()
    {
        if( has_error() )
        {
            KMAP_THROW_EXCEPTION_MSG( error_message() );
        }
    }

    auto value()
        -> T&
    {
        return result.value();
    }

    auto value_or_throw()
        -> T&
    {
        throw_on_error();

        return result.value();
    }

    operator kmap::Result< T >()
    {
        return result;
    }

    kmap::Result< T > result;
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

    auto error_message() const
        -> std::string
    {
        return to_string( result.error() );
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

    auto throw_on_error()
    {
        if( has_error() )
        {
            fmt::print( "[log] kmap::binding::Result::throw_on_error():\n", error_message() );

            KMAP_THROW_EXCEPTION_MSG( error_message() );
        }
    }

    kmap::Result< void > result;
};

} // namespace kmap::binding

#endif // KMAP_EMCC_BINDINGS_HPP