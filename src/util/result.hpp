/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_RESULT_HPP
#define KMAP_UTIL_RESULT_HPP

#include <common.hpp>
#include <util/log/log.hpp>
#include <util/macro.hpp>
#include <utility.hpp>

#include <boost/json.hpp>

#include <concepts>
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
// #define KM_RESULT_PUSH( key, value ) \
//     km_result_local_state.log.push( { key, value } );
#define KM_RESULT_PUSH( key, value ) \
    km_result_local_state.log.push( { key, kmap::result::to_log_value( value ) } );
#define KM_RESULT_PUSH_NODE( name, id ) \
    km_result_local_state.log.push( { name, id } );
    // km_result_local_state.log.push( { name, { kmap::result::dump_about( kmap::Singleton::instance(), id ) } } );
#define KM_RESULT_PUSH_STR( name, str ) \
    km_result_local_state.log.push( { name, str } );
    // km_result_local_state.log.push( { name, { kmap::result::LocalLog::MultiValue{ str, {} } } } );

namespace kmap::result {

class LocalLog
{
public:
    struct MultiValue
    {
        using ValueVariant = std::variant< char const*
                                         , std::string
                                         , Uuid
                                         , std::vector< MultiValue > >;

        std::string key = {};
        ValueVariant value = {};
    };

private:
    std::vector< MultiValue > kvs = {};
#if KMAP_LOG
    util::log::ScopedFunctionLog scoped_log_;
#endif // KMAP_LOG

public:
#if KMAP_LOG
    LocalLog( const char* function
            , const char* file
            , unsigned line ); /* TODO: replace with std::source_location */
    LocalLog() = default;
#endif // KMAP_PROFILE
#if 0 // TODO: This belongs in util::log::ScopedFunctionLog, actually. Or, rather, as a separate logging scoped class, that could be used with scoped function.
    std::string const func_name;
    std::chrono::time_point< std::chrono::system_clock > start_time = std::chrono::system_clock::now();
    LocalLog( const char* function = __builtin_FUNCTION() /* TODO: replace with std::source_location */ );
    ~LocalLog();
#endif

    auto push( MultiValue const& mv )
        -> void;
    auto values() const
        -> std::vector< MultiValue > const&;
};

struct LocalState
{
#if KMAP_LOG
    LocalState( const char* function = __builtin_FUNCTION() /* TODO: replace with std::source_location */
              , const char* file = __builtin_FILE() /* TODO: replace with std::source_location */
              , unsigned line = __builtin_LINE() ); /* TODO: replace with std::source_location */
#else
    LocalState() = default;
#endif // KMAP_LOG

    LocalLog log;
};

auto dump_about( Kmap const& km
               , Uuid const& node )
    -> std::vector< LocalLog::MultiValue >;
auto to_json( LocalLog::MultiValue const& mv )
    -> boost::json::object;
auto to_json( std::vector< LocalLog::MultiValue > const& mvs )
    -> boost::json::array;
auto to_string( LocalLog const& state )
    -> std::string;
auto to_string( LocalLog::MultiValue const& mv )
    -> std::string;
auto to_string( std::vector< LocalLog::MultiValue > const& mvs )
    -> std::string;
template< typename T >
auto to_log_value( T const& t )
    -> LocalLog::MultiValue::ValueVariant
{
    if constexpr( std::convertible_to< T, std::string >
               || std::convertible_to< T, Uuid >
               || std::convertible_to< T, std::vector< LocalLog::MultiValue > >)
    {
        return { t };
    }
    else
    {
        using std::to_string;

        return { to_string( t ) };
    }
}

/**
 * @brief 
 * 
 * @note Avoid auto-deduction to access underlying reference. That is, use `B& b = KTRY( result::dyn_cast< B >( a ) );` over `auto b = KTRY( result::dyn_cast< B >( a ) );`,
 *       as the latter will result in `std::reference_wrapper< B >`, whereas the former will implicitly convert to `B&`.
 * 
 * @param from 
 * @return kmap::Result< std::reference_wrapper< To > > 
 */
template< typename To 
        , typename From >
auto dyn_cast( From const& from )
    -> kmap::Result< std::reference_wrapper< To > >
{
    if( auto const p = dynamic_cast< To* >( from )
      ; p )
    {
        return std::reference_wrapper{ *p };
    }
    else
    {
        return KMAP_MAKE_ERROR_MSG( error_code::common::conversion_failed, "downcast failed" );
    }
}

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