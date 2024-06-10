/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_TAG_HPP
#define KMAP_PATH_NODE_VIEW2_TAG_HPP

#include <common.hpp>
#include <path/view/derivation_link.hpp>
#include <path/view/direct_desc.hpp>

#include <optional>
#include <memory>

namespace kmap::view2 {

// Note:
//     There are two ways to look at view2::tag. One is that it refers to the alias destinations under <node>.$.tag. The other is that it refers to the alias sources.
//     That is, `<node> | tag` results descendant nodes of `/meta.tag`. I've decided `view2::tag` will refer to the latter. The reason this way was chosen has to do with
//     predicate usage. 
class Tag : public DerivationLink
{
    using PredVariant = std::variant< char const*
                                    , std::string
                                    , Uuid
                                    , Tether
                                    // , LinkPtr
                                     >;

    std::optional< PredVariant > pred_ = std::nullopt; 

public:
    Tag() = default;
    virtual ~Tag() = default;

    auto operator()( PredVariant const& pred ) const { auto nl = *this; nl.pred_ = pred; return nl; };
    // Think I only need this if LinkPtr taken as predicate.
    // template< typename LinkType >
    //     requires std::derived_from< LinkType, Link >
    // auto operator()( LinkType const& pred ) const { auto nl = *this; nl.pred_ = LinkPtr{ std::make_unique< LinkType >( pred ) }; return nl; };

    auto clone() const -> std::unique_ptr< Link > override { return { std::make_unique< std::decay_t< decltype( *this ) > >( *this ) }; }
    auto create( CreateContext const& ctx, Uuid const& root ) const -> Result< UuidSet > override;
    auto fetch( FetchContext const& ctx, Uuid const& node ) const -> Result< FetchSet > override;
    auto new_link() const -> std::unique_ptr< Link > override;
    auto pred() const -> std::optional< PredVariant > const& { return pred_; }
    auto to_string() const -> std::string override;

protected:
    auto compare_less( Link const& rhs ) const -> bool override;
};

auto const tag = Tag{};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_TAG_HPP
