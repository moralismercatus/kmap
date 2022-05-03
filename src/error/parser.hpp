/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_PARSER_HPP
#define KMAP_EC_PARSER_HPP

#include "../common.hpp"

namespace kmap::error_code
{

enum class parser 
{
    success = 0
,   parse_failed
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::parser > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class parser_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "parser error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch ( static_cast< parser >( c ) )
        {
        case parser::success: return "success";
        case parser::parse_failed: return "parse_failed";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto parser_category()
    -> kmap::error_code::detail::parser_category const& 
{
  static kmap::error_code::detail::parser_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( parser ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::parser_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_PARSER_HPP