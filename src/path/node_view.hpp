/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW_HPP
#define KMAP_PATH_NODE_VIEW_HPP

#include "common.hpp"
#include "error/network.hpp"
#include "lineage.hpp"
#include "util/concepts.hpp"

#include <optional>

namespace kmap::view
{

enum class flag
{
    create
};

struct all_of
{
    template< typename... Args >
    all_of( Args&&... args )
        : data{ std::forward< Args >( args )... }
    {
    }

    // TODO: Rather than a set< std::string >, this should be a variant, where set< std::string > is an element.
    //       Why? So we can, e.g., use view::child( view::all_of( other_view ) ), or view::child( view::all_of( id_set ) ) just as we would with headings.
    //       Some code that could otherwise be quite straight-forward is awkwardly awaiting this support.
    //       Will require substantial rework, as each operator handles "data", as set< string >, differently.
    //       std::set< std::variant< std::string, Uuid, Intermediate, PlaceholderNode(?) > >
    std::set< std::string > data;
};
struct any_of
{
    template< typename... Args >
    any_of( Args&&... args )
        : data{ std::forward< Args >( args )... }
    {
    }

    std::set< std::string > data;
};
struct none_of
{
    template< typename... Args >
    none_of( Args&&... args )
        : data{ std::forward< Args >( args )... }
    {
    }

    std::set< std::string > data;
};
struct exactly
{
    template< typename... Args >
    exactly( Args&&... args )
        : data{ std::forward< Args >( args )... }
    {
    }

    std::set< std::string > data;
};

struct Alias;
struct Ancestor;
struct Attr;
struct Child;
struct Desc;
struct DirectDesc;
struct Leaf;
struct Parent;
struct Sibling;
struct Single;

using OperatorVariant = std::variant< Alias
                                    , Ancestor
                                    , Attr
                                    , Child
                                    , Desc
                                    , DirectDesc
                                    , Leaf
                                    , Parent
                                    , Sibling
                                    , Single >;

struct Intermediary
{
    std::vector< OperatorVariant > op_chain;
    Uuid root = {};
};

using SelectionVariant = std::variant< char const* // TODO: To consolidate std::string and char const*, and own the char*, consider making SelectionVariant a struct with ctor with variant as a member.
                                     , std::string
                                     , all_of
                                     , any_of
                                     , exactly
                                     , none_of
                                     , Intermediary
                                     , Uuid >;

auto operator==( SelectionVariant const& lhs
               , Heading const& rhs )
    -> bool;
auto operator==( SelectionVariant const& lhs
               , std::set< Heading > const& rhs )
    -> bool;

// Intermediate Operations
struct Ancestor
{
    Ancestor() = default;
    Ancestor( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Ancestor{ sel }; }
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;
    // auto operator()( Intermediary const& i ) const; // TODO: Support composition, e.g.: ancestor( child( "x" ) | parent )

    auto create( Kmap& kmap
               , Uuid const& root ) const
        -> Result< Uuid >;

    std::optional< SelectionVariant > selection = std::nullopt;
};
struct Alias
{
    std::optional< SelectionVariant > selection = std::nullopt;

    Alias() = default;
    Alias( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Alias{ sel }; }
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& parent ) const
        -> Result< UuidSet >;
};
struct Attr
{
    Attr() = default;

    auto operator()() const {}
    // TODO: Doesn't it make more sense to take a single node rather than a set, and let a range operate on it? Return as a set, even if only single possible return, for convenience.
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& parent ) const
        -> Result< Uuid >;
};
struct Child
{
    Child() = default;
    Child( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Child{ sel }; }
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& parent ) const
        -> Result< UuidSet >;

    std::optional< SelectionVariant > selection = std::nullopt;
};
struct Desc
{
    Desc() = default;
    Desc( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Desc{ sel }; }
    auto operator()( OperatorVariant const& op ) const
        -> UuidSet;
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& root ) const
        -> Result< Uuid >;

    std::optional< SelectionVariant > selection = std::nullopt;
};
struct DirectDesc
{
    DirectDesc() = default;
    DirectDesc( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return DirectDesc{ sel }; }
    auto operator()( OperatorVariant const& op ) const
        -> UuidSet;
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& root ) const
        -> Result< Uuid >;

    std::optional< SelectionVariant > selection = std::nullopt;
};
struct Leaf
{
    Leaf() = default;

    auto operator()() {};
    auto operator()( SelectionVariant const& sel ) {};
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    SelectionVariant selection;
};
struct Parent
{
    Parent() = default;

    auto operator()() {};
    auto operator()( SelectionVariant const& sel ) {};
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    SelectionVariant selection;
};
struct Sibling
{
    Sibling() = default;
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto operator()() {};
};
struct Single
{
    Single() = default;

    auto operator()( SelectionVariant const& sel ) {};
    auto operator()() {};
};

// Result Operations
struct Count
{
    Kmap const& kmap;

    Count( Kmap const& km ) : kmap{ km } {}
};
struct Create
{
    Kmap& kmap;

    Create( Kmap& km ) : kmap{ km } {}
};
struct CreateNode
{
    Kmap& kmap;
    
    CreateNode( Kmap& km ) : kmap{ km } {}
};
struct EraseNode
{
    Kmap& kmap;

    EraseNode( Kmap& km ) : kmap{ km } {}
};
struct Exists
{
    Kmap const& kmap;

    Exists( Kmap const& km ) : kmap{ km } {}

    auto operator()() {};
};
struct FetchNode
{
    Kmap const& kmap;

    FetchNode( Kmap const& km ) : kmap{ km } {}

    auto operator()() {};
};
struct FetchOrCreateNode
{
    Kmap& kmap;
};
struct ToHeadingSet
{
    Kmap const& kmap;

    ToHeadingSet( Kmap const& km ) : kmap{ km } {}

    auto operator()() {};
};
struct ToNodeSet
{
    Kmap const& kmap;

    ToNodeSet( Kmap const& km ) : kmap{ km } {}

    auto operator()() {};
};

// Post-Result Operations
struct ToSingle
{
};

auto const alias = Alias{};
auto const ancestor = Ancestor{};
auto const attr = Attr{};
auto const child = Child{};
auto const desc = Desc{};
auto const direct_desc = DirectDesc{};
auto const leaf = Leaf{};
auto const parent = Parent{};
auto const sibling = Sibling{};
auto const single = Single{};

// Post-Result Operations
auto const to_single = ToSingle{};

auto count( Kmap const& kmap ) -> Count;
 // TODO: A bit more on the linguistic debate. "lineage" doesn't roll quite like "node".
 //       Regardless, the fact that all other ops use the singular to represent 1+ (child, alias, attr, erase_node, etc.) means this represents something of a
 //       departure from the norm. I understand the desire: to differentiate a single node returned from multiple. It's a very convenient distiction,
 //       a la fetch_node v. to_node_set. I wonder if there isn't a further refinement to be had e.g., create_node | unique_node?
 //       Trouble here is create_node needs to be an intermediary op _and_ a function(kmap) op to be used in this way. May be doable...
auto create( Kmap const& kmap ) -> Create; // Calls op.create if not found for all in op chain.
auto create_node( Kmap& kmap ) -> CreateNode; // TODO: Should the non-const operators actually belong in the "action" namespace, to be explicit?
auto erase_node( Kmap& kmap ) -> EraseNode;
auto exists( Kmap const& kmap ) -> Exists;
auto fetch_or_create_node( Kmap& kmap ) -> FetchOrCreateNode;
auto fetch_node( Kmap const& kmap ) -> FetchNode;
auto to_heading_set( Kmap const& kmap ) -> ToHeadingSet;
auto to_node_set( Kmap const& kmap ) -> ToNodeSet;

auto root( Uuid const& n ) -> Intermediary;

// template< typename Range >
// auto fetch_unique( Kmap const& kmap ) -> FetchUnique;

// Intermediate Operations
auto operator|( Intermediary const& i, Alias const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Ancestor const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Attr const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Child const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Count const& op ) -> uint32_t;
auto operator|( Intermediary const& i, Desc const& op ) -> Intermediary;
auto operator|( Intermediary const& i, DirectDesc const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Leaf const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Parent const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Sibling const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Single const& op ) -> Intermediary;
// Result Operations
auto operator|( Intermediary const& i, CreateNode const& op ) -> Result< UuidSet >;
auto operator|( Intermediary const& i, EraseNode const& op ) -> Result< void >;
auto operator|( Intermediary const& i, Exists const& op ) -> bool;
auto operator|( Intermediary const& i, FetchNode const& op ) -> Result< Uuid >;
auto operator|( Intermediary const& i, FetchOrCreateNode const& op ) -> Result< Uuid >;
auto operator|( Intermediary const& i, ToHeadingSet const& op ) -> std::set< std::string >; 
auto operator|( Intermediary const& i, ToNodeSet const& op ) -> UuidSet;
// Post-Result Operations
template< concepts::Range RT >
auto operator|( Result< RT > const& result, ToSingle const& tso )
    -> Result< typename RT::value_type >
{
    auto rv = KMAP_MAKE_RESULT( typename RT::value_type );

    if( result )
    {
        auto const& value = result.value();

        if( value.size() == 1 )
        {
            using std::begin;

            rv = *begin( value );
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::network::ambiguous_path );
        }
    }
    else
    {
        rv = KMAP_PROPAGATE_FAILURE( result );
    }

    return rv;
}

auto operator<<( Child const& op
               , flag const& flag )
    -> Child;

auto create_lineages( Kmap& kmap
                    , Uuid const& root
                    , OperatorVariant const& op)
    -> Result< UuidSet >;


} // namespace kmap::view

#endif // KMAP_PATH_NODE_VIEW_HPP
