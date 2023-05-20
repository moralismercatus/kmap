/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_DB_HPP
#define KMAP_EC_DB_HPP

#include "../common.hpp"

#include <boost/system/is_error_code_enum.hpp>

namespace kmap::error_code
{

enum class db
{
    success = 0
,   no_lhs
,   no_rhs
,   push_failed
,   erase_failed
,   entry_not_found
,   update_failed
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::db > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class db_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "db error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch ( static_cast< db >( c ) )
        {
        case db::success: return "success";
        case db::no_lhs: return "no LHS";
        case db::no_rhs: return "no RHS";
        case db::push_failed: return "push failed";
        case db::erase_failed: return "erase failed";
        case db::entry_not_found: return "entry not found";
        case db::update_failed: return "update failed";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto db_category()
    -> kmap::error_code::detail::db_category const& 
{
  static kmap::error_code::detail::db_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( db ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::db_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_DB_HPP