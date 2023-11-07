/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EC_NODE_MANIP_HPP
#define KMAP_EC_NODE_MANIP_HPP

#include <error/master.hpp>

#include <boost/system/error_code.hpp>

#include <type_traits>

namespace kmap::error_code
{

enum class canvas
{
    success = 0
,   conflicts
,   invalid_pane
,   invalid_parent 
,   invalid_subdiv
,   subdiv_already_exists
};

enum class command 
{
    success = 0
,   kscript_unsupported
,   fn_publication_failed 
,   not_general_command
,   not_particular_command
,   incorrect_arg_number 
,   nonarg_node_found
,   invalid_arg 
};

enum class create_alias
{
    success = 0
,   src_not_found
,   dst_not_found
,   src_equals_dst
,   src_ancestor_of_dst
,   alias_already_exists
};

enum class create_node
{
    success = 0
,   invalid_parent
,   node_already_exists
,   duplicate_child_heading
};

enum class node // TODO: It seems there's overlap (redundancy) between "node", "create_node", and "network". Look into this.
{
    success = 0
,   not_found
,   not_descendant
,   not_lineal
,   parent_not_found
,   child_not_found
,   ambiguous
,   invalid_heading
,   invalid_path
,   is_root
,   is_ancestor
,   is_lineal
,   is_nontoplevel_alias 
,   invalid_alias
,   invalid_uuid 
,   invalid_mirror
};

} // namespace kmap::error_code

namespace boost::system
{

template <>
struct is_error_code_enum< kmap::error_code::canvas > : std::true_type
{
};
template <>
struct is_error_code_enum< kmap::error_code::command > : std::true_type
{
};
template <>
struct is_error_code_enum< kmap::error_code::create_alias > : std::true_type
{
};
template <>
struct is_error_code_enum< kmap::error_code::create_node > : std::true_type
{
};
template <>
struct is_error_code_enum< kmap::error_code::node > : std::true_type
{
};
 
} // namespace boost::system

namespace kmap::error_code::detail
{

class canvas_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "canvas error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch( static_cast< canvas >( c ) )
        {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case canvas::success: return "success";
        case canvas::conflicts: return "subdivision conflicts with existing subdivision";
        case canvas::invalid_pane: return "invalid pane";
        case canvas::invalid_parent: return "invalid parent";
        case canvas::invalid_subdiv: return "invalid subdivision";
        case canvas::subdiv_already_exists: return "subdivision already exists";
        }
    }
};
class command_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "command error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch( static_cast< command >( c ) )
        {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case command::success: return "success";
        case command::kscript_unsupported: return "kscript unsupported";
        case command::fn_publication_failed: return "function publication failed";
        case command::not_general_command: return "not general command node";
        case command::not_particular_command: return "not particular command node";
        case command::incorrect_arg_number: return "incorrect number of argument";
        case command::nonarg_node_found: return "non-argument node found";
        case command::invalid_arg: return "invalid argument";
        }
    }
};
class create_alias_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "alias creation error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch( static_cast< create_alias >( c ) )
        {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case create_alias::success: return "alias successfully created";
        case create_alias::src_not_found: return "source not found";
        case create_alias::dst_not_found: return "destination not found";
        case create_alias::src_equals_dst: return "source and destination are the same";
        case create_alias::src_ancestor_of_dst: return "source is ancestor of destination";
        case create_alias::alias_already_exists: return "alias already exists";
        }
    }
};
class create_node_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "node creation error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch( static_cast< create_node >( c ) )
        {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case create_node::success: return "node successfully created";
        case create_node::invalid_parent: return "parent is invalid";
        case create_node::node_already_exists: return "node already exists";
        case create_node::duplicate_child_heading: return "child heading already exists";
        }
    }
};
class node_category : public boost::system::error_category
{
public:
    // Return a short descriptive name for the category
    virtual const char* name() const noexcept override final { return "node error"; }
    // Return what each enum means in text
    virtual std::string message( int c ) const override final
    {
        using namespace kmap::error_code;

        switch( static_cast< node >( c ) )
        {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case node::success: return "node operation successful";
        case node::not_found: return "node not found";
        case node::not_descendant: return "node is not a descendant of ancestor";
        case node::not_lineal: return "node is not lineal to ancestor";
        case node::child_not_found: return "child node not found";
        case node::parent_not_found: return "parent node not found";
        case node::ambiguous: return "node ambiguous";
        case node::invalid_heading: return "invalid node heading";
        case node::invalid_path: return "invalid node heading path";
        case node::is_root: return "is root";
        case node::is_ancestor: return "is ancestor";
        case node::is_lineal: return "is lineal";
        case node::is_nontoplevel_alias: return "is non-top-level alias";
        case node::invalid_alias: return "invalid alias";
        case node::invalid_uuid: return "invalid uuid";
        case node::invalid_mirror: return "invalid mirror";
        }
    }
};

} // namespace kmap::error_code::detail

// Note: Ensure this is in global scope
extern inline
auto canvas_category()
    -> kmap::error_code::detail::canvas_category const& 
{
  static kmap::error_code::detail::canvas_category c;

  return c;
}
extern inline
auto command_category()
    -> kmap::error_code::detail::command_category const& 
{
  static kmap::error_code::detail::command_category c;

  return c;
}
extern inline
auto create_alias_category()
    -> kmap::error_code::detail::create_alias_category const& 
{
  static kmap::error_code::detail::create_alias_category c;

  return c;
}
extern inline
auto create_node_category()
    -> kmap::error_code::detail::create_node_category const& 
{
  static kmap::error_code::detail::create_node_category c;

  return c;
}
extern inline
auto node_category()
    -> kmap::error_code::detail::node_category const& 
{
  static kmap::error_code::detail::node_category c;

  return c;
}
        
namespace kmap::error_code
{
// Note: make_error_code must be declared in same namespace as enum, probably for ADL reasons, but I don't fully understand.

inline
auto make_error_code( canvas ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::canvas_category() };
}
inline
auto make_error_code( command ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::command_category() };
}
inline
auto make_error_code( create_alias ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::create_alias_category() };
}
inline
auto make_error_code( create_node ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::create_node_category() };
}
inline
auto make_error_code( node ec )
    -> boost::system::error_code 
{
  return { static_cast< int >( ec )
         , ::node_category() };
}

} // namespace kmap::error_code

#endif // KMAP_EC_NODE_MANIP_HPP