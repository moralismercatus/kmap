/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_IO_HPP
#define KMAP_IO_HPP

#include "common.hpp"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <ostream>
#include <utility>

// WARNING: Due to idiosynchrasies of ADL, these must go before inclusion of files using these s.a. optional_io.hpp.

namespace kmap::io {

using fmt::format;
using fmt::print;

} // namespace kmap

namespace boost {

// template< typename T > 
// auto operator<<( std::ostream& os
//                , std::unique_ptr< T > const& lhs ) 
//     -> std::ostream&
// { 
//     if( lhs )
//     {
//         os << "->(" << *lhs << " )"; 
//     }

//     return os; 
// } 

template< typename F
        , typename S > 
auto operator<<( std::ostream& os
               , std::pair< F, S > const& lhs ) 
    -> std::ostream&
{ 
    os << "( "
       << lhs.first << ", " 
       << lhs.second << " )"; 

    return os; 
} 

} // namespace boost

namespace fmt {

template <>
struct formatter< kmap::Uuid >
{
    constexpr auto parse( format_parse_context& ctx )
    {
        // assert( false && "unimplemented" );

        // TODO: I believe I want ctx.begin() here, not end().
        return ctx.end();
    }

    template< typename FormatContext >
    auto format( kmap::Uuid const& id
               , FormatContext& ctx )
    {
        return format_to( ctx.out()
                        , boost::uuids::to_string( id ) );
    }
};

} // namespace fmt

#endif // KMAP_COMMON_HPP
