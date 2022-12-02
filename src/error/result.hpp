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

namespace kmap::result {

struct Log
{
    std::vector< std::string > msg_stack = {};
};


// TODO: belongs in kmap ns?
template< typename T
        , typename U >
    requires std::convertible_to< U, T >
auto value_or( kmap::Result< T > const& result
             , U const& u )
    -> T
{
    if( result )
    {
        return result.value();
    }
    else
    {
        return u;
    }
}

} // namespace kmap::result

#endif // KMAP_ERROR_RESULT_HPP