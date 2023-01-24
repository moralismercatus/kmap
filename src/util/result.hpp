/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_RESULT_HPP
#define KMAP_UTIL_RESULT_HPP

#include "common.hpp"
#include "util/macro.hpp"
#include "utility.hpp"

#include <chrono>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace kmap {
    class Kmap;
}

#define KM_RESULT_PROLOG() \
    auto km_result_local_state = kmap::result::LocalState{};
#define KM_RESULT_PUSH( key, value ) \
    km_result_local_state.log.push( { key, value } );
#define KM_RESULT_PUSH_NODE( name, id ) \
    km_result_local_state.log.push( { name, id } );
    // km_result_local_state.log.push( { name, { kmap::result::dump_about( kmap::Singleton::instance(), id ) } } );
#define KM_RESULT_PUSH_STR( name, str ) \
    km_result_local_state.log.push( { name, str } );
    // km_result_local_state.log.push( { name, { kmap::result::LocalLog::MultiValue{ str, {} } } } );

namespace kmap::result {

struct LocalLog
{
    struct MultiValue
    {
        using ValueVariant = std::variant< char const*, std::string, Uuid, std::vector< MultiValue > >;

        std::string key = {};
        ValueVariant value = {};
    };

   std::vector< MultiValue > kvs = {};

#if KMAP_PROFILE_LOG
    std::string const func_name;
    std::chrono::time_point< std::chrono::system_clock > start_time = std::chrono::system_clock::now();
    LocalLog( const char* function = __builtin_FUNCTION() /* TODO: replace with std::source_location */ );
    ~LocalLog();
#else
    LocalLog() = default;
#endif // KMAP_PROFILE

    auto push( MultiValue const& mv )
        -> void;
};

struct LocalState
{
    LocalLog log;
};

auto dump_about( Kmap const& km
               , Uuid const& node )
    -> std::vector< LocalLog::MultiValue >;
auto to_string( LocalLog const& state )
    -> std::string;
auto to_string( std::vector< LocalLog::MultiValue > const& mvs )
    -> std::string;

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

} // namespace kmap::result

#endif // KMAP_UTIL_RESULT_HPP