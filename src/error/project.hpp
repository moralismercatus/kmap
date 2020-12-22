/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_PROJECT_HPP
#define KMAP_EC_PROJECT_HPP

#include "../common.hpp"

namespace kmap::error_code
{

enum class project
{
    success = 0
,   invalid_project
,   ambiguous_status
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::project > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class project_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "project error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch ( static_cast< project >( c ) )
        {
        case project::success: return "success";
        case project::invalid_project: return "invalid project";
        case project::ambiguous_status: return "ambiguous status";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto project_category()
    -> kmap::error_code::detail::project_category const& 
{
  static kmap::error_code::detail::project_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( project ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::project_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_PROJECT_HPP