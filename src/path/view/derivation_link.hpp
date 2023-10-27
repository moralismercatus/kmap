/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_DERIVATION_LINK_HPP
#define KMAP_PATH_NODE_VIEW2_DERIVATION_LINK_HPP

#include <common.hpp>
#include <path/view/link.hpp>
#include <util/polymorphic_value.hpp>
#include <util/result.hpp>

#include <compare>
#include <concepts>
#include <typeindex>
#include <typeinfo>

namespace kmap::view2 {

class DerivationLink : public Link
{
public:
    virtual ~DerivationLink() = default;

    virtual auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > = 0;
    virtual auto fetch( FetchContext const& ctx, Uuid const& root ) const -> Result< FetchSet > = 0;
};

auto fetch( Link const* link
          , FetchContext const& ctx
          , Uuid const& node )
    -> Result< FetchSet >;
auto fetch( PolymorphicValue< Link > const& plink
          , FetchContext const& ctx
          , Uuid const& node )
    -> Result< FetchSet >;

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_DERIVATION_LINK_HPP
