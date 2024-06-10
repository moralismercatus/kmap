/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_PARENT_HPP
#define KMAP_PATH_NODE_VIEW2_PARENT_HPP

#include <common.hpp>

#include <path/view/derivation_link.hpp>
#include <path/view/tether.hpp>

#include <optional>
#include <memory>
#include <variant>

namespace kmap::view2 {

class Parent : public DerivationLink
{
    using PredVariant = std::variant< char const*
                                    , std::string
                                    , Uuid
                                    , Tether >;

    std::optional< PredVariant > pred_ = std::nullopt; 

public:
    Parent() = default;
    virtual ~Parent() = default;

    auto operator()( PredVariant const& pred ) const { auto nl = *this; nl.pred_ = pred; return nl; };
    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> Result< FetchSet > override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const parent = Parent{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_PARENT_HPP
