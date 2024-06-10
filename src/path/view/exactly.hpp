/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_EXACTLY_HPP
#define KMAP_PATH_NODE_VIEW2_EXACTLY_HPP

#include "common.hpp"
#include "path/view/common.hpp"
#include "path/view/qualifier_link.hpp"

#include <memory>

namespace kmap::view2 {

class Exactly : public QualifierLink< Exactly >
{
public:
    using QualifierLink::QualifierLink;
    virtual ~Exactly() = default;

    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> Result< FetchSet > override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& other ) const -> bool override;
};

auto const exactly = Exactly{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_EXACTLY_HPP
