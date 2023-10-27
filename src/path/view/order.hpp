/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ORDER_HPP
#define KMAP_PATH_NODE_VIEW2_ORDER_HPP

#include <common.hpp>
#include <path/view/transformation_link.hpp>
#include <path/view/tether.hpp>

#include <optional>
#include <memory>
#include <variant>

namespace kmap::view2 {

class Order : public TransformationLink
{
public:
    Order() = default;
    virtual ~Order() = default;

    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto fetch( FetchContext const& ctx, FetchSet const& fs ) const -> Result< FetchSet > override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const order = Order{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_ORDER_HPP
