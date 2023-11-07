/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_TAINT_HPP
#define KMAP_EC_TAINT_HPP

#include "../common.hpp"

namespace kmap::error_code
{

enum class taint
{
    success = 0
,   invalid_address
,   invalid_inst_index
,   invalid_backlink_offset
,   invalid_project
,   origin_not_found 
,   backlink_not_found
,   data_load_failed 
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::taint > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class taint_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "taint error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch( static_cast< taint >( c ) )
        {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case taint::success: return "success";
        case taint::invalid_address: return "invalid address";
        case taint::invalid_inst_index: return "invalid instruction index";
        case taint::invalid_backlink_offset: return "invalid backlink offset";
        case taint::invalid_project: return "not valid taint project";
        case taint::origin_not_found: return "origin not found";
        case taint::backlink_not_found: return "backlink not found";
        case taint::data_load_failed: return "failed to load data";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto taint_category()
    -> kmap::error_code::detail::taint_category const& 
{
  static kmap::error_code::detail::taint_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( taint ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::taint_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_TAINT_HPP