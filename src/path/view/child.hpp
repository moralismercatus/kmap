/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_CHILD_HPP
#define KMAP_PATH_NODE_VIEW2_CHILD_HPP

#include "common.hpp"
#include "path/view/link.hpp"

#include <optional>
#include <memory>

namespace kmap::view2 {

class Child : public Link
{
    using PredVariant = std::variant< char const*
                                    , std::string
                                    , Uuid
                                    // Tether?
                                    // Chain?
                                    // Link?
                                     >;

    std::optional< PredVariant > pred_ = std::nullopt; 

public:
    Child() = default;
    virtual ~Child() = default;

    auto operator()( PredVariant const& pred ) const { auto nl = *this; nl.pred_ = pred; return nl; };
    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> FetchSet override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto pred() const -> std::optional< PredVariant > const& { return pred_; }
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const child = Child{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_CHILD_HPP
