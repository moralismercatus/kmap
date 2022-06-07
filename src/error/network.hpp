/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_NETWORK_HPP
#define KMAP_EC_NETWORK_HPP

#include "../common.hpp"

namespace kmap::error_code
{

enum class network
{
    success = 0
,   ambiguous_path
,   attribute
,   child_already_exists
,   duplicate_node 
,   invalid_heading
,   invalid_lineage
,   invalid_instance
,   invalid_edge
,   invalid_node 
,   invalid_ordering
,   invalid_parent
,   invalid_path // Ambiguity between this and invalid_node. Yes, a node that isn't found at syntactically valid path X is "invalid", but the path is still a "valid" path.
,   invalid_root
,   no_prev_selection
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::network > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class network_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "network error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch ( static_cast< network >( c ) )
        {
        case network::success: return "success";
        case network::ambiguous_path: return "path is ambiguous";
        case network::attribute: return "attribute";
        case network::child_already_exists: return "child already exists";
        case network::duplicate_node: return "duplicate_node";
        case network::invalid_heading: return "invalid heading";
        case network::invalid_lineage: return "invalid lineage";
        case network::invalid_instance: return "invalid instance";
        case network::invalid_edge: return "invalid edge";
        case network::invalid_node: return "invalid node";
        case network::invalid_ordering: return "invalid ordering";
        case network::invalid_parent: return "invalid parent";
        case network::invalid_path: return "invalid path; path syntax not valid";
        case network::invalid_root: return "invalid root";
        case network::no_prev_selection: return "no previous node selected";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto network_category()
    -> kmap::error_code::detail::network_category const& 
{
  static kmap::error_code::detail::network_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( network ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::network_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_NETWORK_HPP