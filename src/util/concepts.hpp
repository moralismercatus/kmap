/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_CONCEPT_HPP
#define KMAP_UTIL_CONCEPT_HPP

#include <concepts>
#include <tuple>
// #include <ranges>

namespace kmap::concepts {

template< typename T >
concept StdPair = requires( T t )
{
	t.first;
	t.second;
};

template< typename T >
concept TuplePair = requires( T t )
{
    requires std::tuple_size< T >::value == 2;
    std::get< 0 >( t );
    std::get< 1 >( t );
};

template< typename T >
concept Pair = StdPair< T > || TuplePair< T >;

template< typename T >
concept Result = requires( T t )
{
    // TODO: { t }             -> std::convertible_to< bool >;
    t.has_value();
    t.has_error();
    t.error();
};

template< typename T >
concept Range = requires( T t ) // TODO: Deprecate when emscripten std::ranges::range (libc-13) lands.
{
    std::begin( t );
    std::end( t );
};

template< typename T >
concept RangeResult = Result< T > && Range< typename T::value_type >;
template< typename T >
concept NonRangeResult = !RangeResult< T >;

template< typename T >
concept Boolean = requires( T t )
{
    static_cast< bool >( t );
};

// template< typename T >
// concept MultiRhsTable = requires( T t )
// {
//     typename T::left_type;
//     { t.fetch( typename T::left_type{} ) } -> RangeResult;
// };

// template< typename T >
// concept UniqueRhsTable = !MultiRhsTable< T >;

// template< typename T >
// concept UniqueRhsTable = requires( T t )
// {
//     !MultiRhsTable< T >;
// };

}

#endif // KMAP_UTIL_CONCEPT_HPP