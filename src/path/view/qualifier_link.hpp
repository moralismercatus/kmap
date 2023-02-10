/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_QUALIFIER_LINK_HPP
#define KMAP_PATH_NODE_VIEW2_QUALIFIER_LINK_HPP

#include "common.hpp"
#include "path/view/concepts.hpp"
#include "path/view/link.hpp"
#include "util/concepts.hpp"

#include "path/view/act/to_string.hpp"

#include <concepts>
#include <initializer_list>
#include <memory>
#include <variant>
#include <vector>

namespace kmap::view2 {

template< typename Derived >
class QualifierLink : public Link
{
    using Links = std::vector< LinkPtr >;
    using Tethers = std::vector< Tether >;
    using Underlying = std::variant< Links, Tethers >;

    Links links_ = {};

public:
    QualifierLink() = default;
    QualifierLink( Links const& links )
        : links_{ links }
    {
    }
    virtual ~QualifierLink() = default;

    auto links() const -> Links const& { return links_; }

    template< typename... Args >
        requires concepts::LinkArgs< Args... >
    auto operator()( Args&&... args ) const
    {
        auto nlinks = Links{};
        ( nlinks.emplace_back( std::make_unique< Args >( std::forward< Args >( args ) ) ), ... );
        return Derived{ nlinks };
    }
    template< typename Range >
        requires std::derived_from< typename Range::value_type, Link >
              && kmap::concepts::Range< Range >
    auto operator()( Range const& rng ) const
    {
        using LinkType = std::decay_t< typename Range::value_type >;
        auto links = Links{};
        for( auto const& e : rng )
        {
            links.emplace_back( std::make_unique< LinkType >( e ) );
        }
        return Derived{ links };
    };
    template< typename RLink
            , typename Range >
        requires std::derived_from< RLink, Link >
              && kmap::concepts::Range< Range >
    auto operator()( RLink const& rlink, Range const& rng ) const
    {
        auto links = Links{};
        for( auto const& e : rng )
        {
            links.emplace_back( LinkPtr{ std::make_unique< RLink >( RLink{}( e ) ) } );
        }
        return Derived{ links };
    };
    template< typename RLink
            , typename IListType >
        requires std::derived_from< RLink, Link >
    auto operator()( RLink const& rlink, std::initializer_list< IListType > const& ilist ) const
    {
        auto links = Links{};
        for( auto const& e : ilist )
        {
            links.emplace_back( LinkPtr{ std::make_unique< RLink >( RLink{}( e ) ) } );
        }
        return Derived{ links };
    };

    virtual auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > = 0;
    virtual auto fetch( FetchContext const& ctx, Uuid const& node ) const -> FetchSet = 0;
};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_QUALIFIER_LINK_HPP
