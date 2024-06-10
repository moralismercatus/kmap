
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_LINEAGE_HPP
#define KMAP_LINEAGE_HPP

#include "common.hpp"

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/iterator/insert_iterators.hpp>

#include <utility>

namespace kmap {

class Kmap;

// TODO: What's the difference between this and LinealRange, in principle?
//       The ability to have the lineage in either direction? That's one difference.
//       But I suspect these should be merged.
// Note: I'm deprecating for now, as I believe there isn't sufficient difference between this and LinealRange.

class [[ deprecated( "consider using LinealRange instead" ) ]] Lineage
{
public:
    Lineage() = default;

    auto make( Kmap const& kmap
             , UuidVec const& nodes )
        -> Result< Lineage >;

    auto begin() const // TODO: Should be cbegin()?
        -> UuidVec::const_iterator;
    auto end() const
        -> UuidVec::const_iterator;

    auto is_ascending() const
        -> bool;
    auto is_descending() const
        -> bool;
    auto reverse()
        -> Lineage&;
    auto to_vector() const
        -> UuidVec const&;

protected:
    Lineage( UuidVec const& nodes
           , bool const& ascending );

private:
    UuidVec nodes_;
    bool ascending_ = true;
};

// Purpose of this class is to constrain an ancestor-descendant relation.
// At the time of the creation of an instance of this class, the caller can be
// Assured that such a lineal relationship exists.
// For this reason, LinealPair should probably not be stored long term.
// TODO: Consider sanity checking on access that relationship still exists.
// TODO: I think one way to "ensure" that a Lineal node remains so throughout the lifetime of the Lineal object is to provide a "Context" with Lineal that
//       contains a `Kmap const&`, and expect that only that const kmap reference is used along with Lineal.
class Lineal // TOOD: `public : ExistentNode`, where ExistentNode guarantees a node's existence upon creation.
{
public:
    static 
    auto make( Kmap const& kmap
             , Uuid const& root
             , Uuid const& leaf )
        -> Result< Lineal >;

    auto root() const
        -> Uuid const&;
    auto leaf() const
        -> Uuid const&;

    template< size_t I >
    auto get() const
    {
        if      constexpr( I == 0 ) { return root_; }
        else if constexpr( I == 1 ) { return leaf_; }
        else                        { static_assert( I >= 0 && I < 2 ); }
    }

protected:
    Lineal( Kmap const& kmap
          , Uuid const& root
          , Uuid const& leaf );

private:
    std::reference_wrapper< Kmap const > kmap_;
    Uuid root_;
    Uuid leaf_;
};


class Descendant : public Lineal
{
public:
    static 
    auto make( Kmap const& kmap
             , Uuid const& ancestor
             , Uuid const& descendant )
        -> Result< Descendant >;

protected:
    Descendant( Kmap const& kmap
              , Uuid const& root
              , Uuid const& leaf );
};

using Ancestor = Descendant;

/**
 *  TODO: Rename to Lineage? Replace existing kmap::Lineage? Replace existing kmap::Lineage => UuidVec?
 *  TODO: Biggest problem with this "range" is that it's eager rather than lazy. 
 *        In fact, I'm not entirely sure how to do "lazy", when nodes only have "parent" relationships. 
 **/
// TODO: I don't see much justification for this class, in the presence of the (lazy) view:: mechanism.
//       Usage can be replaced with anchor::node( desc ) | view2::ancestor... actually, I'm not sure there's currently a mechanism to replace this.
//       Predicates for ancestor, direct_desc, right_lineal, etc., don't give a lineage between [a,d], but rather a specific node.
//       This may prompt a change in interface. Maybe right/left_lineal should be distinct from right/left_lineage. Where lineage( n ) results in all nodes
//       up to n, inclusive. Perhaps likewise a view::ancestor v. view::ancestry distinction.
class [[ deprecated( "prefer passing node views" ) ]] LinealRange
{
public:
    using value_type = Uuid;

    LinealRange() = default;
    // LinealRange( LinealRange const& other );

    static
    auto make( Kmap const& kmap
             , Lineal const& lin )
        -> Result< LinealRange >;

    auto begin()
        -> UuidVec::iterator;
    auto begin() const
        -> UuidVec::const_iterator;
    auto end()
        -> UuidVec::iterator;
    auto end() const
        -> UuidVec::const_iterator;

    // TODO: This shouldn't be exposed to the public, rather, only e.g., ranges::back_inserter
    //       shoud be allowed to call this, where the view elements come from a LinealRange.
    template< typename T >
    auto push_back( T&& t ) // Required for ranges::copy()
        -> void
    {
        lineage_.push_back( std::forward< T >( t ) );
    }

protected:
    LinealRange( UuidVec const& lineage );

private:
    UuidVec lineage_ = {};
};

template< typename In >
auto operator|( In&& in, LinealRange&& rhs )
    -> LinealRange
{
    ranges::copy( std::forward< In >( in ), ranges::back_inserter( rhs ) );

    return rhs;
}

// TODO: This is a temporary workaround until I can figure out how to connect ranges::to< LinealRange >().
inline
auto to_lineal_range()
    -> LinealRange
{
    return LinealRange{};
}

auto to_heading_path( Kmap const& kmap
                    , LinealRange const& lr )
    -> std::string;

} // namespace kmap

namespace kmap::network::node {

/**
 * Note: The recommendation is against storage of Mapped types. This is because a Mapped type is merely the present constraint.
 *       At any time, a mapped type could become "unmapped" (deleted), thus invalidating the constraint.
 **/
class Mapped // Went with term "Mapped" over "Existent", as even an unmapped node can "exist."
{
public:
    Mapped( Kmap const& kmap
          , Uuid const& id );

    auto id() const // Throws if unmapped at call site. In theory, assertion would be appropriate, but in practice...
        -> Uuid const&;

private:
    std::reference_wrapper< Kmap const > kmap_;
    Uuid const id_;
};

// TODO: Presumably, the likes of Lineal and Descendant would take Mapped as a parameter.
//       Something I'm noticing is that all of these constrained types seem to make the code less error prone,
//       but more obtuse to code. E.g., `TRY( fetch_parent( root, id ) )` becomes `get_parent( TRY( make< Descendant >( mapped_root, mapped_desc ) ) )` ) 
//       They both fail if 'id' is not a desc of root, and the error propagated, but the latter is much more obtuse to use.
//       Unless, of course, you already have a Descendant type, in which case it becomes less obtuse: `get_parent( desc )`.

} // namespace network::node

namespace std {

template<>
struct tuple_size< kmap::Lineal >
{
    static const std::size_t value = 2;
};

template< size_t I >
class tuple_element< I, kmap::Lineal > {
public:
    using type = decltype( declval< kmap::Lineal >().get< I >() );
};

template<>
struct tuple_size< kmap::Descendant >
{
    static const std::size_t value = 2;
};

template< size_t I >
class tuple_element< I, kmap::Descendant > {
public:
    using type = decltype( declval< kmap::Descendant >().get< I >() );
};

} // namespace std

#endif // KMAP_LINEAGE_HPP