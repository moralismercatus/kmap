/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_JS_IFACE_HPP
#define KMAP_EC_JS_IFACE_HPP

#include "../common.hpp"

namespace kmap::error_code
{

enum class js 
{
    success = 0
,   eval_failed
,   element_already_exists 
,   invalid_element
,   js_exception
,   lint_failed
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::js > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class js_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "javascript error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch( static_cast< js >( c ) )
        {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case js::success: return "success";
        case js::eval_failed: return "eval failed";
        case js::element_already_exists: return "element already exists";
        case js::invalid_element: return "invalid element";
        case js::js_exception: return "js exception";
        case js::lint_failed: return "linting failed";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto js_category()
    -> kmap::error_code::detail::js_category const& 
{
  static kmap::error_code::detail::js_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( js ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::js_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_JS_IFACE_HPP