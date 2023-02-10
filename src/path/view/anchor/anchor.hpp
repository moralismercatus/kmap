/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ANCHOR_HPP
#define KMAP_PATH_NODE_VIEW2_ANCHOR_HPP

#include "path/view/common.hpp"
// #include "path/view/tether.hpp"
#include "util/polymorphic_value.hpp"

#include <concepts>
#include <memory>
#include <typeindex>
#include <typeinfo>

namespace kmap::view2 {

class Anchor
{
public:
    using AnchorPtr = PolymorphicValue< Anchor >;

    virtual ~Anchor() = default;
    virtual auto clone() const -> std::unique_ptr< Anchor > = 0;
    virtual auto fetch( FetchContext const& ctx ) const -> FetchSet = 0;
    virtual auto to_string() const -> std::string = 0;
    auto operator<( Anchor const& other ) const -> bool { return compare_less( other ); }

    // operator Tether() const { return Tether{ clone() }; }

protected:
    virtual auto compare_less( Anchor const& other ) const -> bool = 0;
};

template< typename Lhs >
    requires std::derived_from< Lhs, Anchor >
auto compare_anchors( Lhs const& lhs
                    , Anchor const& rhs )
    -> std::strong_ordering
{
    auto const rhs_id = std::type_index( typeid( rhs ) );
    auto const lhs_id = std::type_index( typeid( lhs ) );

    // TODO: When Emscripten supports std::type_index::operator<=>:
    // return std::type_index( typeid( *this ) ) <=> std::type_index( typeid( other ) );
    // ...
    // It appears Emscripten hasn't implemented std::type_index::operator<=> yet.
    // So... until it does, need to unpack various operators:

         if( lhs_id < rhs_id ){ return std::strong_ordering::less; }
    else if( lhs_id > rhs_id ){ return std::strong_ordering::greater; }
    else                      { return std::strong_ordering::equal; }
}

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_ANCHOR_HPP
