/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ALIAS_HPP
#define KMAP_PATH_NODE_VIEW2_ALIAS_HPP

#include <path/view/derivation_link.hpp>
#include <path/view/tether.hpp>

#include <optional>
#include <memory>
#include <variant>

namespace kmap::view2 {

class Alias : public DerivationLink
{
    using PredVariant = std::variant< char const*
                                    , std::string
                                    , Uuid
                                    , LinkPtr
                                    , Tether >;

    std::optional< PredVariant > pred_ = std::nullopt; 

public:
    Alias() = default;
    virtual ~Alias() = default;

    auto operator()( PredVariant const& pred ) const { auto nl = *this; nl.pred_ = pred; return nl; };
    // template< typename AnchorType
    //         , typename TailLink >
    // auto operator()( TetherCT< AnchorType, TailLink > const& pred ) const { auto nl = *this; nl.pred_ = pred | to_tether; return nl; };
    template< typename LinkType >
        requires std::derived_from< LinkType, Link >
    auto operator()( LinkType const& pred ) const { auto nl = *this; nl.pred_ = LinkPtr{ std::make_unique< LinkType >( pred ) }; return nl; };
    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> Result< FetchSet > override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto pred() const -> decltype( pred_ ) const& { return pred_; }
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const alias = Alias{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_ALIAS_HPP
