/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_CHAIN_HPP
#define KMAP_PATH_NODE_VIEW2_CHAIN_HPP

#include "path/view/link.hpp"

#include <concepts>
#include <deque>
#include <memory>

namespace kmap::view2 {

#if 0
struct Chain
{
    std::deque< std::shared_ptr< Link > > links = {};

    auto operator<( Chain const& other ) const -> bool;
    // std::strong_ordering operator<=>( Chain const& other ) const;
};

auto operator<<( std::ostream& os
               , Chain const& chain )
    -> std::ostream&;

template< typename LLink
        , typename RLink >
    requires std::derived_from< LLink, Link >
          && std::derived_from< RLink, Link >
auto operator|( LLink const& lhs
              , RLink const& rhs )
    -> Chain 
{
    auto rv = Chain{};

    rv.links = { std::make_shared< LLink >( lhs ), std::make_shared< RLink >( rhs ) };

    return rv;
}

template< typename RLink >
    requires std::derived_from< RLink, Link >
auto operator|( Chain const& lhs
              , RLink const& rhs )
    -> Chain
{
    lhs.links.emplace_back( std::make_shared< RLink >( rhs ) );
}

template< typename LLink >
    requires std::derived_from< LLink, Link >
auto operator|( LLink const& lhs
              , Chain const& rhs )
    -> Chain
{
    rhs.links.emplace_front( std::make_shared< LLink >( lhs ) );

    return rhs;
}
#endif // 0

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_CHAIN_HPP
