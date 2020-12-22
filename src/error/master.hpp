/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
// TODO: Rename to error/common.hpp?
#pragma once
#ifndef KMAP_EC_MASTER_HPP
#define KMAP_EC_MASTER_HPP

// TODO: Replace macros with std::source_location. This can be done by creating a default
//       argument KMAP_MAKE_RESULT( T >( boost::system::error_code const& ec = common::unknown, std::source_location const& = std::source_location::current() )
// TODO: Once std::source_location becomes available, exchange all macros for functions.
#define KMAP_MAKE_RESULT_STACK_ELEM() \
    kmap::error_code::StackElement{ __LINE__ \
                                  , __PRETTY_FUNCTION__ \
                                  , __FILE__ }
#define KMAP_MAKE_ERROR_MSG( ec, msg ) \
    kmap::error_code::Payload{ ( ec ) \
                             , ( msg ) \
                             , { KMAP_MAKE_RESULT_STACK_ELEM() } }
#define KMAP_MAKE_ERROR_UUID( ec, id ) KMAP_MAKE_ERROR_MSG( ec, io::format( "ID: {}\n", id ) )
#define KMAP_MAKE_ERROR( ec ) KMAP_MAKE_ERROR_MSG( ec, "" )
#define KMAP_MAKE_RESULT_EC( type, ec ) Result< type >{ KMAP_MAKE_ERROR( ( ec ) ) }
#define KMAP_MAKE_RESULT( type ) KMAP_MAKE_RESULT_EC( type, kmap::error_code::common::unknown  )
#define KMAP_ENSURE_MSG( rv, pred, ec, msg ) \
    { \
        auto&& res = ( pred ); \
        if( !( res ) ) \
        { \
            ( rv ) = KMAP_MAKE_ERROR_MSG( ( ec ), ( msg ) ); \
            return ( rv ).as_failure(); \
        } \
    }
#define KMAP_ENSURE( rv, pred, ec ) KMAP_ENSURE_MSG( ( rv ), ( pred ), ( ec ), "" )
#define KMAP_PROPAGATE_FAILURE( ... ) \
    ({ \
        auto res = ( __VA_ARGS__ ); \
        res.error().stack.emplace_back( KMAP_MAKE_RESULT_STACK_ELEM() ); \
        res.as_failure(); \
    })
// Inspired by BOOST_OUTCOME_TRYX, with the addition of appending to the stack.
#define KMAP_TRY( ... ) \
    ({ \
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

#define KMAP_THROW_EXCEPTION_MSG( msg ) \
    ({ \
        fmt::print( stderr, "exception:\n\tmessage: {}\n\tfunction: {}\n\tfile: {}\nline: {}\n", ( msg ), __PRETTY_FUNCTION__, __FILE__, __LINE__ ); \
        throw std::runtime_error( ( msg ) ); \
    })

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

namespace kmap::error_code {

// TODO: Replace StackElement with std::source_location
struct StackElement
{
    uint32_t line = {};
    std::string function = {};
    std::string file = {};
};

struct Payload
{
    boost::system::error_code ec = {}; // TODO: Any good reason to choose boost over std error_code?
    std::string message = {};
    std::vector< StackElement > stack = {}; // TODO: Replace StackElement with std::source_location
};

template< typename T
        , typename E = Payload
        , typename NoValuePolicy = outcome::policy::default_policy< T, E, void > >
using Result = outcome::result< T
                              , E
                              , NoValuePolicy >;

enum class common
{
    unknown = 1 // 0 should never be an error.
,   data_not_found
,   conversion_failed
};

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

    ss << fmt::format( "error: {}\n"
                       "message: {}\n"
                       "result stack:\n"
                     , sp.ec.message()
                     , sp.message );

    for( auto const& e : sp.stack )
    {
        ss << fmt::format( "\t{}|{}|{}\n"
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
    fmt::print( stderr, "{}\n", to_string( payload ) );

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
        case common::unknown: return "unknown error";
        case common::data_not_found: return "data not found";
        case common::conversion_failed: return "conversion failed";
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


#endif // KMAP_EC_MASTER_HPP