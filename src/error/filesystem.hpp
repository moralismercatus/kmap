/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_FILESYSTEM_HPP
#define KMAP_EC_FILESYSTEM_HPP

#include "../common.hpp"

namespace kmap::error_code
{

enum class filesystem
{
    success = 0
,   file_not_found
,   file_read_failure
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::filesystem > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class filesystem_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "filesystem error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch ( static_cast< filesystem >( c ) )
        {
        case filesystem::success: return "success";
        case filesystem::file_not_found: return "file not found";
        case filesystem::file_read_failure: return "error occurred while attempting to read from file";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto filesystem_category()
    -> kmap::error_code::detail::filesystem_category const& 
{
  static kmap::error_code::detail::filesystem_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( filesystem ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::filesystem_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_FILESYSTEM_HPP