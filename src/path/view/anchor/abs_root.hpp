/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ANCHOR_ABSROOT_HPP
#define KMAP_PATH_NODE_VIEW2_ANCHOR_ABSROOT_HPP

#include "path/view/anchor/anchor.hpp"
#include "path/view/tether.hpp"

namespace kmap::view2::anchor {

class AbsRoot : public Anchor
{
public:
    virtual ~AbsRoot() = default;

    auto clone() const -> std::unique_ptr< Anchor > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto fetch( FetchContext const& ctx ) const -> FetchSet override;
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Anchor const& anchor ) const -> bool override;
};

auto const abs_root = AbsRoot{};

} // namespace kmap::view2::anchor

#endif // KMAP_PATH_NODE_VIEW2_ANCHOR_ABSROOT_HPP
