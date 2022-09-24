/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
// TODO: Rename to error/common.hpp?
#pragma once
#ifndef KMAP_EC_MASTER_HPP
#define KMAP_EC_MASTER_HPP

#if KMAP_DEBUG
    #define KMAP_LOG_EXCEPTION 1
    #define KMAP_DISABLE_EXCEPTION_LOG_SCOPED() auto const disable_exception_log = kmap::log::ScopedDisabler{ kmap::log::flag::log_exception }
#else
    #define KMAP_DISABLE_EXCEPTION_LOG_SCOPED() (void)0;
#endif // kmap::log

#define KMAP_THROW_EXCEPTION_MSG( msg ) \
    ({ \
        kmap::log_exception( ( msg ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ); \
        throw std::runtime_error( fmt::format( "Exception:\n\tmessage: {}\n\t{}|{}|{}", ( msg ), __LINE__, __PRETTY_FUNCTION__, __FILE__ ) ); \
    })
// TODO: Replace macros with std::source_location. This can be done by creating a default
//       argument KMAP_MAKE_RESULT( T >( boost::system::error_code const& ec = common::uncategorized, std::source_location const& = std::source_location::current() )
// TODO: Once std::source_location becomes available, exchange all macros for functions.
#define KMAP_MAKE_RESULT_STACK_ELEM_MSG( msg ) \
    kmap::error_code::StackElement{ __LINE__ \
                                  , __PRETTY_FUNCTION__ \
                                  , __FILE__ \
                                  , ( msg ) }
#define KMAP_MAKE_RESULT_STACK_ELEM() KMAP_MAKE_RESULT_STACK_ELEM_MSG( "" )
#define KMAP_MAKE_ERROR_MSG( ec, msg ) \
    kmap::error_code::Payload{ ( ec ) \
                             , { KMAP_MAKE_RESULT_STACK_ELEM_MSG( msg ) } }
#define KMAP_MAKE_ERROR_UUID( ec, id ) KMAP_MAKE_ERROR_MSG( ec, io::format( "ID: {}\n", id ) )
#define KMAP_MAKE_ERROR( ec ) KMAP_MAKE_ERROR_MSG( ec, "" )
#define KMAP_MAKE_RESULT_EC( type, ec ) Result< type >{ KMAP_MAKE_ERROR( ( ec ) ) }
#define KMAP_MAKE_RESULT( type ) KMAP_MAKE_RESULT_EC( type, kmap::error_code::common::uncategorized  )
#define KMAP_MAKE_RESULT_MSG( type, msg ) Result< type >{ KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, msg ) }
#define KMAP_PROPAGATE_FAILURE_MSG( result, msg ) \
    ({ \
        auto res = ( result ); \
        res.error().stack.emplace_back( KMAP_MAKE_RESULT_STACK_ELEM_MSG( ( msg ) ) ); \
        res.as_failure(); \
    })
#define KMAP_PROPAGATE_FAILURE( ... ) \
    ({ \
        auto res = ( __VA_ARGS__ ); \
        res.error().stack.emplace_back( KMAP_MAKE_RESULT_STACK_ELEM() ); \
        res.as_failure(); \
    })
#define KMAP_ENSURE_MSG( pred, ec, msg ) \
    { \
        auto&& res = ( pred ); \
        if( !( res ) ) \
        { \
            { \
                return ensure_propagate_error( res, ec, KMAP_MAKE_RESULT_STACK_ELEM_MSG( ( fmt::format( "predicate: {}\n\tmessage: {}", #pred, msg ) ) ) ); \
            } \
        } \
    }
#define KMAP_ENSURE( pred, ec ) KMAP_ENSURE_MSG( ( pred ), ( ec ), "" )
// Inspired by BOOST_OUTCOME_TRYX, with the addition of appending to the stack.
#if KMAP_LOG_KTRY
    #define KMAP_KTRY_LOG() kmap::log_ktry_line( __FUNCTION__, __LINE__, __FILE__ )
#else 
    #define KMAP_KTRY_LOG()
#endif // KMAP_LOG_KTRY
#if KMAP_LOG_KTRYE
    #define KMAP_KTRYE_LOG() kmap::log_ktrye_line( __FUNCTION__, __LINE__, __FILE__ )
#else 
    #define KMAP_KTRYE_LOG()
#endif // KMAP_LOG_KTRYE
#define KMAP_TRY( ... ) \
    ({ \
        KMAP_KTRY_LOG(); \
        auto&& res = ( __VA_ARGS__ ); \
        if( BOOST_OUTCOME_V2_NAMESPACE::try_operation_has_value( res ) ) \
            ; \
        else \
        { \
            res.error().stack.emplace_back( KMAP_MAKE_RESULT_STACK_ELEM() ); \
            return BOOST_OUTCOME_V2_NAMESPACE::try_operation_return_as( static_cast< decltype( res )&& >( res ) ); \
        } \
        BOOST_OUTCOME_V2_NAMESPACE::try_operation_extract_value( static_cast< decltype( res )&& >( res ) ); \
    })
// Throw exception on failure.
#define KMAP_TRYE( ... ) \
    ({ \
        auto&& res = ( __VA_ARGS__ ); \
        if( BOOST_OUTCOME_V2_NAMESPACE::try_operation_has_value( res ) ) \
            ; \
        else \
        { \
            KMAP_KTRYE_LOG(); \
            res.error().stack.emplace_back( KMAP_MAKE_RESULT_STACK_ELEM() ); \
            KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
        } \
        BOOST_OUTCOME_V2_NAMESPACE::try_operation_extract_value( static_cast< decltype( res )&& >( res ) ); \
    })
#define KMAP_ENSURE_EXCEPT( pred ) \
    { \
        if( !( pred ) ) \
        { \
            KMAP_THROW_EXCEPTION_MSG( #pred ); \
        } \
    }
#define KTRY( ... ) KMAP_TRY( __VA_ARGS__ )
#define KTRYE( ... ) KMAP_TRYE( __VA_ARGS__ )

#include <boost/outcome.hpp>
#include <boost/system/error_code.hpp>
#include <fmt/format.h>

#include <cstdarg> // Needed for __VA_ARGS__
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <stdexcept>

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

#if KMAP_DEBUG || 1
namespace kmap::log
{
    namespace flag // Allows dynamic switch.
    {
        inline auto log_exception = true;
    }

    struct ScopedDisabler
    {
        bool& flag_;
        bool prev_value;
        ScopedDisabler( bool& flag ) : flag_{ flag }, prev_value{ flag } { flag_ = false; }
        ~ScopedDisabler(){ flag_ = prev_value; }
    };
} // kmap::log
#endif // KMAP_DEBUG

namespace kmap {

inline
auto log_exception( std::string const& msg
                  , std::string const& fn
                  , std::string const& file
                  , uint64_t const& line )
    -> void
{
#if KMAP_LOG_EXCEPTION
    if( kmap::log::flag::log_exception )
    {
        fmt::print( stderr, "exception:\n\tmessage: {}\n\tfunction: {}\n\tfile: {}\nline: {}\n", msg, fn, file, line );
    }
#endif
}

}
namespace kmap::error_code {

// TODO: Replace StackElement with std::source_location
struct StackElement
{
    uint32_t line = {};
    std::string function = {};
    std::string file = {};
    std::string message = {};
};

struct Payload
{
    boost::system::error_code ec = {}; // TODO: Any good reason to choose boost over std error_code?
    std::vector< StackElement > stack = {};
};

template< typename T
        , typename E = Payload
        , typename NoValuePolicy = outcome::policy::default_policy< T, E, void > >
using Result = outcome::result< T
                              , E
                              , NoValuePolicy >;

enum class common
{
    uncategorized = 1 // 0 should never be an error.
,   data_already_exists
,   data_not_found
,   conversion_failed
,   invalid_numeric
};

// This is a helper function to propagate a Result<> if input is Result<> or make one if the input is bool.
template< typename Pred
        , typename ErrorCode >
auto ensure_propagate_error( Pred const& pred
                           , ErrorCode const& ec
                           , kmap::error_code::StackElement const& se )
{
    if constexpr( requires{ pred.as_failure(); } )
    {
        auto tpred = pred;
        // TODO: what about msg? Would stack element need a message?
        tpred.error().stack.emplace_back( se );
        return tpred.as_failure();
    }
    else
    {
        return kmap::error_code::Payload{ ec, { se } };
    }
}

inline 
auto make_error_code( Payload const& sp )
    -> boost::system::error_code
{
     return sp.ec;
}

inline
auto to_string( Payload const& sp )
    -> std::string
{
    std::stringstream ss;

    ss << fmt::format( "category: {}\n"
                       "item: {}\n"
                       "result stack:\n"
                     , sp.ec.category().name()
                     , sp.ec.message() );

    for( auto const& e : sp.stack )
    {
        ss << fmt::format( "\tmessage: {}\n{}|{}|{}\n"
                         , e.message
                         , e.line
                         , e.function
                         , e.file );
    }

    return ss.str();
}

inline 
auto outcome_throw_as_system_error_with_payload( Payload payload )
    -> void
{
#if KMAP_DEBUG
    if( kmap::log::flag::log_exception )
    {
        fmt::print( stderr, "{}\n", to_string( payload ) );
    }
#endif

    KMAP_THROW_EXCEPTION_MSG( to_string( payload ) );
}

} // namespace kmap::error_code

namespace boost::system {

template <>
struct is_error_code_enum< kmap::error_code::common > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail {

class common_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "common error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch ( static_cast< common >( c ) )
        {
        case common::uncategorized: return "uncategorized";
        case common::data_already_exists: return "data already exists";
        case common::data_not_found: return "data not found";
        case common::conversion_failed: return "conversion failed";
        case common::invalid_numeric: return "invalid numeric";
        }
    }
};

} // kmap::error_code::detail

// Global ns
extern inline
auto common_category()
    -> kmap::error_code::detail::common_category const& 
{
  static kmap::error_code::detail::common_category c;

  return c;
}


namespace kmap::error_code {

inline
auto make_error_code( common ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::common_category() };
}


template< typename T >
auto operator<<( std::ostream& os, kmap::error_code::Result< T > const& lhs )
    -> std::ostream&
{
    if( lhs )
    {
        constexpr bool has_to_string = requires( T const& t )
        {
            to_string( t );
        };

        if constexpr( has_to_string )
        {
            os << to_string( lhs.value() );
        }
        else
        {
            os << "result valid, no to_string";
        }
    }
    else
    {
        os << lhs.error().ec.message();
    }

    return os;
}

} // namespace kmap::error_code

namespace kmap
{
    auto format_line_log( std::string const& func
                        , uint32_t const& line
                        , std::string const& file )
        -> std::string;
    auto log_ktry_line( std::string const& func
                      , uint32_t const& line
                      , std::string const& file )
        -> void;
    auto log_ktrye_line( std::string const& func
                       , uint32_t const& line
                       , std::string const& file )
        -> void;
} // namespace kmap

#endif // KMAP_EC_MASTER_HPP