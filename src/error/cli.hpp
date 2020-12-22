/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_CLI_HPP
#define KMAP_EC_CLI_HPP

#include "../common.hpp"

namespace kmap::error_code
{

enum class cli 
{
    success = 0
,   failure
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::cli > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class cli_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "cli error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch ( static_cast< cli >( c ) )
        {
        case cli::success: return "success";
        case cli::failure: return "failure";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto cli_category()
    -> kmap::error_code::detail::cli_category const& 
{
  static kmap::error_code::detail::cli_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( cli ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::cli_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_CLI_HPP