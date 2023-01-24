/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ANY_OF_HPP
#define KMAP_PATH_NODE_VIEW2_ANY_OF_HPP

#include "common.hpp"
#include "path/view/common.hpp"
#include "path/view/link.hpp"
#include "path/view/qualifier_link.hpp"

#include <memory>

namespace kmap::view2 {

class AnyOf : public QualifierLink< AnyOf >
{
public:
    using QualifierLink::QualifierLink;
    virtual ~AnyOf() = default;

    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> FetchSet override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const any_of = AnyOf{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_ANY_OF_HPP
