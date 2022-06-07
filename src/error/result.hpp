/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_ERROR_RESULT_HPP
#define KMAP_ERROR_RESULT_HPP

#include "error/master.hpp"
#include "utility.hpp"

namespace kmap::error {

template< typename T >
auto make_result( kmap::error_code::Payload const& payload )
    -> kmap::Result< T >
{
    return Result< T >{ payload };
}

template< typename T >
auto make_result( const char* function = __builtin_FUNCTION() /* TODO: replace with std::source_location */
                , const char* file = __builtin_FILE() /* TODO: replace with std::source_location */
                , unsigned line = __builtin_LINE() ) /* TODO: replace with std::source_location */
    -> kmap::Result< T >
{
    auto const payload = kmap::error_code::Payload{ kmap::error_code::common::uncategorized
                                                  , { kmap::error_code::StackElement{ line
                                                                                    , function
                                                                                    , file
                                                                                    , "" } } };

    return make_result< T >( payload );
}

template< typename T >
auto make_result( Kmap const& kmap
                , Uuid const& node
                , const char* function = __builtin_FUNCTION() /* TODO: replace with std::source_location */
                , const char* file = __builtin_FILE() /* TODO: replace with std::source_location */
                , unsigned line = __builtin_LINE() ) /* TODO: replace with std::source_location */
    -> kmap::Result< T >
{
    auto const msg = to_string_elaborated( kmap, node );
    auto const payload = kmap::error_code::Payload{ kmap::error_code::common::uncategorized
                                                  , { kmap::error_code::StackElement{ line
                                                                                    , function
                                                                                    , file
                                                                                    , msg } } };

    return make_result< T >( payload );
}

template< typename T >
auto value_or( kmap::Result< T > const& result
             , T const& t )
    -> T
{
    if( result )
    {
        return result.value();
    }
    else
    {
        return t;
    }
}

} // namespace kmap::error

#endif // KMAP_ERROR_RESULT_HPP