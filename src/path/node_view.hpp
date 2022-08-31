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

#include <functional>
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
    //       Actually, it probably needs to be an "std::set< std::any >", just a matter of how to evaluate the "any":
    //       I suppose in terms of the caller, something like: if constexpr( pred::all_of ){ return ranges::all_of( data, intermedary | op ) }...
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

struct AbsRoot;
struct Alias;
struct Ancestor;
struct Attr;
struct Child;
struct Desc;
struct DirectDesc;
struct Leaf;
struct Lineage;
struct RLineage;
struct Parent;
struct Resolve;
struct Sibling;
struct SiblingIncl;
struct Single;
struct Tag;

using OperatorVariant = std::variant< AbsRoot
                                    , Alias
                                    , Ancestor
                                    , Attr
                                    , Child
                                    , Desc
                                    , DirectDesc
                                    , Leaf
                                    , Lineage
                                    , RLineage
                                    , Parent
                                    , Resolve
                                    , Sibling
                                    , SiblingIncl
                                    , Single
                                    , Tag >;

struct Intermediary
{
    std::vector< OperatorVariant > op_chain;
    UuidSet root = {};
};

using PredFn = std::function< bool(Uuid const&) >;
// TODO: Rename SelectionVariant to PredicateVariant?
using SelectionVariant = std::variant< char const* // TODO: To consolidate std::string and char const*, and own the char*, consider making SelectionVariant a struct with ctor with variant as a member.
                                     , std::string
                                     , all_of
                                     , any_of
                                     , exactly
                                     , none_of
                                     , Intermediary
                                     , Uuid
                                     , UuidSet
                                     , PredFn >;

auto to_string( SelectionVariant const& sel )
    -> std::string;

auto operator==( SelectionVariant const& lhs
               , Heading const& rhs )
    -> bool;
auto operator==( SelectionVariant const& lhs
               , std::set< Heading > const& rhs )
    -> bool;

// Intermediate Operations
struct AbsRoot
{
    AbsRoot() = default;

    auto operator()() const {}
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto to_string() const
        -> std::string;
};
struct Ancestor
{
    std::optional< SelectionVariant > selection = std::nullopt;

    Ancestor() = default;
    explicit Ancestor( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Ancestor{ sel }; }
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;
    // auto operator()( Intermediary const& i ) const; // TODO: Support composition, e.g.: ancestor( child( "x" ) | parent )

    auto create( Kmap& kmap
               , Uuid const& root ) const
        -> Result< Uuid >;
    auto to_string() const
        -> std::string;
};
struct Alias
{
    using Src = StrongType< Uuid, class SrcTag >;
    using Dst = StrongType< Uuid, class DstTag >;
    using PredVariant = std::variant< char const*
                                    , std::string
                                    , Intermediary // Equivalent to UuidSet, right?
                                    , Uuid
                                    , UuidSet // Equivalent to all_of{ ids }, right?
                                    , PredFn
                                    , Src
                                    , Dst >;
    std::optional< PredVariant > predicate = std::nullopt;

    Alias() = default;
    explicit Alias( PredVariant const& pred ) : predicate{ pred } {}

    auto operator()() const {}
    auto operator()( PredVariant const& pred ) const { return Alias{ pred }; } // TODO: Replace all operator() used for selection to fetch(), or something equivalent. Explicit name.
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& parent ) const
        -> Result< UuidSet >;
    auto to_string() const
        -> std::string;
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
    auto to_string() const
        -> std::string;
};
struct Child
{
    Child() = default;
    explicit Child( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Child{ sel }; }
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& parent ) const
        -> Result< UuidSet >;
    auto to_string() const
        -> std::string;

    std::optional< SelectionVariant > selection = std::nullopt;
};
struct Desc
{
    Desc() = default;
    explicit Desc( SelectionVariant const& sel ) : selection{ sel } {}

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
    auto to_string() const
        -> std::string;

    std::optional< SelectionVariant > selection = std::nullopt;
};
struct DirectDesc
{
    std::optional< SelectionVariant > selection = std::nullopt;

    DirectDesc() = default;
    explicit DirectDesc( SelectionVariant const& sel ) : selection{ sel } {}

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
    auto to_string() const
        -> std::string;
};
struct Leaf
{
    std::optional< SelectionVariant > selection = std::nullopt;

    Leaf() = default;

    auto operator()() {};
    auto operator()( SelectionVariant const& sel ) {};
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto to_string() const
        -> std::string;
};
struct Lineage // = Ancestor + current node
{
    std::optional< SelectionVariant > selection = std::nullopt;

    Lineage() = default;
    explicit Lineage( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Lineage{ sel }; }
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& root ) const
        -> Result< Uuid >;
    auto to_string() const
        -> std::string;
};
struct RLineage
{
    Uuid leaf;

    RLineage() = default;
    explicit RLineage( Uuid const& leafn ) : leaf{ leafn } {};

    auto operator()() {};
    auto operator()( Uuid const& leafn ) const { return RLineage{ leafn }; }; // TODO: Rather, SelectionVariant?
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto to_string() const
        -> std::string;
};
struct Parent
{
    std::optional< SelectionVariant > selection = std::nullopt;

    Parent() = default;

    auto operator()() {};
    auto operator()( SelectionVariant const& sel ) {};
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto to_string() const
        -> std::string;
};
struct Resolve
{
    Resolve() = default;

    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto operator()() {};

    auto to_string() const
        -> std::string;
};
struct Sibling
{
    Sibling() = default;

    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto operator()() {};

    auto to_string() const
        -> std::string;
};
struct SiblingIncl
{
    SiblingIncl() = default;

    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto operator()() {};

    auto to_string() const
        -> std::string;
};
struct Single
{
    Single() = default;

    auto operator()( SelectionVariant const& sel ) {};
    auto operator()() {};

    auto to_string() const
        -> std::string;
};
struct Tag
{
    std::optional< SelectionVariant > selection = std::nullopt;

    Tag() = default;
    explicit Tag( SelectionVariant const& sel ) : selection{ sel } {}

    auto operator()() const {}
    auto operator()( SelectionVariant const& sel ) const { return Tag{ sel }; }
    auto operator()( Kmap const& kmap
                   , Uuid const& node ) const
        -> UuidSet;

    auto create( Kmap& kmap
               , Uuid const& parent ) const
    -> Result< UuidSet >;
    auto to_string() const
        -> std::string;
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
struct ToAbsPath
{
    Kmap const& kmap;

    ToAbsPath( Kmap const& km ) : kmap{ km } {}

    auto operator()() {};
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

extern Intermediary const abs_root;
auto const alias = Alias{};
auto const ancestor = Ancestor{};
auto const attr = Attr{};
auto const child = Child{};
auto const desc = Desc{};
auto const direct_desc = DirectDesc{};
auto const leaf = Leaf{};
auto const lineage = Lineage{}; // Ancestor + current (i.e., "ancestor_inclusive")
auto const rlineage = RLineage{}; // TODO: "Lineage" doesn't specify direction. It assumes ascending, but why not descending? A "lineage" can go both ways. Perhaps l_lineage, r_lineage? (left,right/asc/desc?)
auto const parent = Parent{};
auto const resolve = Resolve{};
auto const sibling = Sibling{};
auto const sibling_incl = SiblingIncl{};
// TODO: auto const sibling_incl = view::parent | view::child; // TODO: How to make sibling_incl capable of consuming predicates and dispatching them to RHS e.g., sibling_incl( "victor" )?
auto const single = Single{};
auto const tag = Tag{};

// Post-Result Operations
auto const to_single = ToSingle{};

// TODO: Should actions take Kmap&, or individual components?
//       e.g., `view::child | to_node_set( nw )` and `view::child | view::tag | to_node_set( nw, tstore )`
//       It is not to say that the Kmap& option wouldn't still be available.
//       Why might this less convenient, more verbose requirement be preferable?
//       It is a way of ensuring that components are initialized as dependents among dependents.
//       For example, MyComponent : public Component< Network, TagStore >
//       wherein Component provides accessors for network() and tag_store(), and also auto-registers them as requisites.
//       In this way, by convention, components only access other components via the Component::<accessor> way (perhaps not even making kmap_inst() available)
//       and calling actions with those accessor instances to perform all operations. In effect, Components would always auto-register all their requisites they use.
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
auto to_abs_path( Kmap const& kmap ) -> ToAbsPath;
auto to_heading_set( Kmap const& kmap ) -> ToHeadingSet;
auto to_node_set( Kmap const& kmap ) -> ToNodeSet;

auto make( Uuid const& n ) -> Intermediary;
auto make( UuidSet const& n ) -> Intermediary;

// template< typename Range >
// auto fetch_unique( Kmap const& kmap ) -> FetchUnique;

// Intermediate Operations
// Need here: operator( op, op ) => Intermediary (fine, could be called view::node, I reckon)
auto operator|( Intermediary const& i, Alias const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Ancestor const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Attr const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Child const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Count const& op ) -> uint32_t;
auto operator|( Intermediary const& i, Desc const& op ) -> Intermediary;
auto operator|( Intermediary const& i, DirectDesc const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Leaf const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Lineage const& op ) -> Intermediary;
auto operator|( Intermediary const& i, RLineage const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Parent const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Resolve const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Sibling const& op ) -> Intermediary;
auto operator|( Intermediary const& i, SiblingIncl const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Single const& op ) -> Intermediary;
auto operator|( Intermediary const& i, Tag const& op ) -> Intermediary;
// Actions
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
template< concepts::Range RT >
auto operator|( Result< RT > const& result, ToAbsPath const& op )
    -> Result< UuidVec >
{
    auto rv = KMAP_MAKE_RESULT( UuidVec );

    if( result )
    {
        auto const& value = result.value();

        if( value.size() == 1 )
        {
            rv = KMAP_TRYE( absolute_path( op.kmap, value.back() ) );
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

namespace kmap::view::act {

// TODO: "actions" go here. No more view::create_node, but now act::create_node( kmap ).
//       Actions are what take the recipe (lazy) and act upon it; put it into motion.
//       Particularly useful for name reuse e.g., view::alias( view::count( 3 ) ) v. view::alias | act::count.
// auto count( Kmap const& kmap ) -> Count;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_NODE_VIEW_HPP
