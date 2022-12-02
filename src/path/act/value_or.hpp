/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_VALUE_OR_HPP
#define KMAP_PATH_ACT_VALUE_OR_HPP

#include "error/result.hpp"

namespace kmap::view::act {

template< typename T >
struct ValueOr
{
    T t;
};

template< typename T >
auto value_or( T const& t )
    -> ValueOr< T >
{
    return ValueOr< T >{ t };
}

template< typename T >
auto operator|( kmap::Result< T > const& result, ValueOr< T > const& op )
    -> T
{
    return result::value_or( result, op.t );
}

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}

#endif // KMAP_PATH_ACT_VALUE_OR_HPP
