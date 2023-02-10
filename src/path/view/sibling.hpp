/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_SIBLING_HPP
#define KMAP_PATH_NODE_VIEW2_SIBLING_HPP

#include "common.hpp"
#include "path/view/child.hpp"
#include "path/view/link.hpp"
#include "path/view/parent.hpp"
#include "path/view/tether.hpp"

#include <string>

namespace kmap::view2 {

// Note: `auto const sibling_incl = view2::parent | view2::child` - doesn't work because if no parent, then no child/siblings.

class SiblingIncl : public Link
{
    using PredVariant = std::variant< char const*
                                    , std::string
                                    , Uuid
                                    , LinkPtr
                                    , Tether >;

    std::optional< PredVariant > pred_ = std::nullopt; 

public:
    SiblingIncl() = default;
    virtual ~SiblingIncl() = default;

    auto operator()( PredVariant const& pred ) const { auto nl = *this; nl.pred_ = pred; return nl; };
    template< typename AnchorType
            , typename TailLink >
    auto operator()( TetherCT< AnchorType, TailLink > const& pred ) const { auto nl = *this; nl.pred_ = pred | to_tether; return nl; };
    template< typename LinkType >
        requires std::derived_from< LinkType, Link >
    auto operator()( LinkType const& pred ) const { auto nl = *this; nl.pred_ = LinkPtr{ std::make_unique< LinkType >( pred ) }; return nl; };
    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> FetchSet override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const sibling_incl = SiblingIncl{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_SIBLING_HPP
