/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_LINK_HPP
#define KMAP_PATH_NODE_VIEW2_LINK_HPP

#include "common.hpp"
#include "util/polymorphic_value.hpp"

#include <compare>
#include <concepts>
#include <typeindex>
#include <typeinfo>

namespace kmap::view2 {


class Link
{
public:
    using LinkPtr = PolymorphicValue< Link >;

private:
    LinkPtr prev_ = {};

public:
    virtual ~Link() = default;

    auto prev() -> LinkPtr& { return prev_; }
    auto prev() const -> LinkPtr const& { return prev_; }
    auto prev( LinkPtr&& nprev ) -> void { prev_ = std::move( nprev ); }
    auto prev( LinkPtr const& nprev ) -> void { prev_ = nprev->clone(); }
    virtual auto clone() const -> std::unique_ptr< Link > = 0;
    virtual auto new_link() const -> std::unique_ptr< Link > = 0; // TODO: Should unique_ptr< Link > be replaced with Link&, as all Links, by convention, should have a const global variable that could be returned, right?
    virtual auto to_string() const -> std::string = 0;
    bool operator<( Link const& other ) const;
    // TODO: Enable after Emscripten supports std types for operator<=> (optional, variant, type_info, etc.)
    // std::strong_ordering operator<=>( Link const& rhs ) const { return compare( rhs ); }
    // auto operator==( Link const& rhs ) const -> bool { return compare( rhs ) == 0; }
    // auto operator=( Link const& other )
    //     -> Link&
    // {
    //     // Hmm.. will providing this for a base with a move-only type (unique_ptr) allow derived classes to be copyable? One would think so...
    //     auto tprev = prev_;
    //     auto oprev = other.prev();
    // }

protected:
    virtual auto compare_less( Link const& other ) const -> bool = 0;
};

template< typename Lhs >
    requires std::derived_from< Lhs, Link >
auto compare_links( Lhs const& lhs
                  , Link const& rhs )
    -> std::strong_ordering
{
    auto const lhs_id = std::type_index( typeid( lhs ) );
    auto const rhs_id = std::type_index( typeid( rhs ) ); // Uses RTTI to find derived type of ref.

    // TODO: When Emscripten supports std::type_index::operator<=>:
    // return std::type_index( typeid( *this ) ) <=> std::type_index( typeid( other ) );
    // ...
    // It appears Emscripten hasn't implemented std::type_index::operator<=> yet.
    // So... until it does, need to unpack various operators:

         if( lhs_id < rhs_id ){ return std::strong_ordering::less; }
    else if( lhs_id > rhs_id ){ return std::strong_ordering::greater; }
    else                      { return std::strong_ordering::equal; }
}

template< typename Lhs
        , typename Rhs >
    requires std::derived_from< Lhs, Link >
          && std::derived_from< Rhs, Link >
auto operator|( Lhs const& lhs
              , Rhs const& rhs )
    -> Rhs
{
    auto nrhs = rhs;

    if( auto prev = std::ref( nrhs.prev() )
      ; prev.get() )
    {
        auto last = prev;

        while( prev.get() )
        {
            last = prev;
            prev = std::ref( prev.get()->prev() );
        }

        last.get()->prev( { std::make_unique< Lhs >( lhs ) } );
    }
    else
    {
        nrhs.prev( { std::make_unique< Lhs >( lhs ) } );
    }

    return nrhs;
}

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_LINK_HPP
