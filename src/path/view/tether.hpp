/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_TETHER_HPP
#define KMAP_PATH_NODE_VIEW2_TETHER_HPP

#include "path/view/anchor/anchor.hpp"
#include "path/view/link.hpp"

#include <range/v3/range/conversion.hpp>

#include <concepts>
#include <memory>
#include <vector>

// namespace kmap::view2 {
//     class Anchor;
//     class Link;
// }

namespace kmap::view2 {

class Tether 
{
public:
    Anchor::AnchorPtr anchor; // Guaranteed: has_value()
    Link::LinkPtr tail_link = {};  

    template< typename AnchorType >
        requires std::derived_from< AnchorType, Anchor >
    Tether( AnchorType const& anc )
        : anchor{ Anchor::AnchorPtr{ std::make_unique< AnchorType >( anc ) } }
    {
    }
    Tether( Anchor::AnchorPtr anc );
    Tether( Anchor::AnchorPtr anc
          , Link::LinkPtr tlink );

    auto operator<( Tether const& other ) const -> bool;
    // std::strong_ordering operator<=>( Tether const& other ) const;

    auto to_string() const
        -> std::string;
};

struct ToTether
{
};

auto const to_tether = ToTether{};

template< typename AnchorType
        , typename TailLink >
    requires std::derived_from< AnchorType, Anchor >
          && std::derived_from< TailLink, Link >
class TetherCT
{
public:
    using anchor_type = AnchorType;
    using tail_link_type = TailLink;

    AnchorType anchor;
    TailLink tail_link;

    TetherCT( AnchorType const& anc
            , TailLink const& tlink )
        : anchor{ anc }
        , tail_link{ tlink }
    {
    }

    // TODO: Should this provide an implicit conversion to Tether? May be able to avoid the overload for TetherCT that explicitly does the conversion.
    //       Likewise, a constructor for Tether that takes a TetherCT would provide the implicit conversion, no?
    //       Not sure... can potential introduce subtle bugs, wherein ctor is expected, but instead calls implicit conversion.
    // TODO: Controversial. 
    operator Tether() const
    {
        return *this | to_tether;
    }

    template< typename... Args >
    auto operator()( Args&&... args ) const
        -> TetherCT
    {
        auto t = *this;
        
        t.tail_link = tail_link( std::forward< Args >( args )... );

        return t;
    }

    auto to_string() const
        -> std::string
    {
        return fmt::format( "TetherCT|anchor:TODO|link:TODO" );
    }
};

auto operator<<( std::ostream& os
               , kmap::view2::Tether const& tether )
    -> std::ostream&;

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_TETHER_HPP
