/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_DESC_HPP
#define KMAP_PATH_NODE_VIEW2_DESC_HPP

#include "common.hpp"
#include "path/view/derivation_link.hpp"

#include <optional>
#include <memory>
#include <variant>

namespace kmap::view2 {

class Desc : public DerivationLink
{
    using PredVariant = std::variant< char const*
                                    , std::string
                                    , Uuid
                                    , LinkPtr >;

    std::optional< PredVariant > pred_ = std::nullopt; 
public:

    Desc() = default;
    virtual ~Desc() = default;

    template< typename LinkType >
        requires std::derived_from< LinkType, Link >
    auto operator()( LinkType const& pred ) const { auto nl = *this; nl.pred_ = LinkPtr{ std::make_unique< LinkType >( pred ) }; return nl; };
    auto operator()( PredVariant const& pred ) const { auto nl = *this; nl.pred_ = pred; return nl; };
    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> Result< FetchSet > override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto pred() const -> std::optional< PredVariant > const& { return pred_; }
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const desc = Desc{};


} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_DESC_HPP
