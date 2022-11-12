/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_HPP
#define KMAP_PATH_NODE_VIEW2_HPP

#include "common.hpp"
#include "path/node_view.hpp"

#include <concepts>
#include <memory>

namespace kmap
{
    class Kmap;
} // namespace kmap

namespace kmap::view2 {

struct Chain;

struct CreateContext
{
    Kmap& km;
    Chain const& chain;
};
struct FetchContext
{
    Kmap const& km;
    Chain const& chain;

    FetchContext( Kmap const& k
                , Chain const& c )
        : km{ k }
        , chain{ c }
    {
    }
    FetchContext( CreateContext const& ctx )
        : km{ ctx.km }
        , chain{ ctx.chain }
    {
    }
};

struct Anchor
{
    virtual auto fetch( FetchContext const& ctx ) const -> UuidSet = 0;
};

struct Link
{
    virtual auto fetch( FetchContext const& ctx, Uuid const& root ) const -> UuidSet = 0;
};

struct PredLink : public Link
{
    mutable std::optional< view::SelectionVariant > selection; // Bit of a hack, but allows selection to be modified despite const vars defs.

    PredLink() = default;
    explicit PredLink( view::SelectionVariant const& sel ) : selection{ sel } {}

    virtual auto operator()( view::SelectionVariant const& sel ) const -> PredLink const& = 0;
};

struct CreatablePredLink : public PredLink // TODO: I think there are cases where non-predicate creatabls exist (e.g., attribute nodes), so Createable shouldn't depend on Pred.c
{
    using PredLink::PredLink;

    virtual auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > = 0;
};

struct ForwardingLink : public CreatablePredLink
{
    std::pair< std::shared_ptr< Link >, std::shared_ptr< Link > > links = {};

    using CreatablePredLink::CreatablePredLink;
    virtual ~ForwardingLink() = default;

    auto operator()( view::SelectionVariant const& sel ) const -> ForwardingLink const& override;

    // Does nothing more than forward arguments to sublinks.
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& root ) const -> UuidSet override;
};

struct Chain 
{
    std::shared_ptr< Anchor > anchor = {};
    std::vector< std::shared_ptr< Link > > links = {};
};

// Can I make these templates? Would it matter? Maybe... 
template< typename LLink
        , typename RLink >
    requires ::concepts::derived_from< LLink, Link >
          && ::concepts::derived_from< RLink, Link >
auto operator|( LLink const& lhs
              , RLink const& rhs )
    -> ForwardingLink
{
    auto rv = ForwardingLink{};

    rv.links = std::pair{ std::make_shared< LLink >( lhs ), std::make_shared< RLink >( rhs ) };

    return rv;
}

template< typename RLink >
    requires ::concepts::derived_from< RLink, Link >
auto operator|( Chain const& lhs
              , RLink const& rhs )
    -> Chain
{
    auto rv = lhs;

    rv.links.emplace_back( std::make_shared< RLink >( rhs ) );

    return rv;
}

// TODO: What happens if I drop independent Anchor types, but auto create Chains for Anchors?
// template< typename LAnchor
//         , typename RLink >
//     requires ::concepts::derived_from< LAnchor, Anchor >
//           && ::concepts::derived_from< RLink, Link >
// auto operator|( LAnchor const& lhs
//               , RLink const& rhs )
//     -> Chain
// {
//     auto rv = Chain{};

//     rv.anchor = std::make_shared< LAnchor >( lhs );
//     rv.links.emplace_back( std::make_shared< RLink >( rhs ) );

//     return rv;
// }

// inline
// auto operator|( std::shared_ptr< Link > const& lhs
//               ,  )

/*----------------Derivatives----------------*/

struct AbsRoot : public Anchor
{
    virtual ~AbsRoot() = default;

    auto fetch( FetchContext const& ctx ) const -> UuidSet override;
};

struct Root : public Anchor
{
    UuidSet nodes;

    Root( UuidSet const& ns )
        : nodes{ ns }
    {
    }
    virtual ~Root() = default;

    auto fetch( FetchContext const& ctx ) const -> UuidSet override;
};

auto const abs_root = Chain{ .anchor = std::make_shared< AbsRoot >() };
// TODO: variadic template...
auto root( Uuid const& node ) -> Chain;
auto root( UuidSet const& nodes ) -> Chain;

struct Child : public PredLink
{
    using PredLink::PredLink;
    virtual ~Child() = default;

    auto operator()( view::SelectionVariant const& sel ) const -> Child const& override;

    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> UuidSet override;
};

auto const child = Child{};

struct Desc : public PredLink
{
    using PredLink::PredLink;
    virtual ~Desc() = default;

    auto operator()( view::SelectionVariant const& sel ) const -> Desc const& override;

    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> UuidSet override;
};

auto const desc = Desc{};

struct DirectDesc : public PredLink
{
    using PredLink::PredLink;
    virtual ~DirectDesc() = default;

    auto operator()( view::SelectionVariant const& sel ) const -> DirectDesc const& override;

    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> UuidSet override;
};

auto const direct_desc = DirectDesc{};

struct AbsPathFlat
{
    Kmap const& km;

    AbsPathFlat( Kmap const& kmap );
};
struct ToNodeSet
{
    Kmap const& km;

    ToNodeSet( Kmap const& kmap );
};

auto abs_path_flat( Kmap const& km )
    -> AbsPathFlat;
auto to_node_set( Kmap const& km )
    -> ToNodeSet;

// auto operator|( Anchor const& lhs
//               , AbsPathFlat const& rhs )
//     -> Result< std::string >;
auto operator|( Chain const& lhs
              , AbsPathFlat const& rhs )
    -> Result< std::string >;
// auto operator|( Anchor const& lhs
//               , ToNodeSet const& rhs )
//     -> UuidSet;
auto operator|( Chain const& lhs
              , ToNodeSet const& rhs )
    -> UuidSet;

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_HPP
