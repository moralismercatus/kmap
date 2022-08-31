/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_ALIAS_STORE_HPP
#define KMAP_ALIAS_STORE_HPP

#include "common.hpp"

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/uuid/uuid_hash.hpp>

#include <set>

namespace bmi = boost::multi_index;

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

[[ nodiscard ]]
auto make_alias_id( Uuid const& alias_src
                  , Uuid const& alias_dst )
    -> Uuid;

struct AliasItem
{
    using alias_type = StrongType< Uuid, class AliasId >;
    using src_type   = StrongType< Uuid, class SrcId >;
    using dst_type   = StrongType< Uuid, class DstId >;

    src_type src_id{};
    dst_type dst_id{};

    auto alias() const { return make_alias_id( src_id.value(), dst_id.value() ); }
    auto src() const { return src_id; }
    auto dst() const { return dst_id; }
};

struct AliasChildItem
{
    using parent_type   = StrongType< Uuid, class ParentId >;
    using child_type    = StrongType< Uuid, class ChildId >;
    using pair_type     = std::pair< parent_type, child_type >;

    parent_type parent_id{};
    child_type child_id{};

    auto parent() const { return parent_id; }
    auto child() const { return child_id; }
    auto pair() const { return std::pair{ parent_id, child_id }; }
};

using AliasSet = boost::multi_index_container< AliasItem
                                             , bmi::indexed_by< bmi::hashed_unique< bmi::tag< AliasItem::alias_type >
                                                                                            , bmi::const_mem_fun< AliasItem
                                                                                                                , Uuid
                                                                                                                , &AliasItem::alias >
                                                                                            , boost::hash< Uuid > >
                                                              , bmi::hashed_non_unique< bmi::tag< AliasItem::src_type >
                                                                                                , bmi::const_mem_fun< AliasItem
                                                                                                                    , AliasItem::src_type
                                                                                                                    , &AliasItem::src >
                                                                                                , boost::hash< AliasItem::src_type > >
                                                              , bmi::hashed_non_unique< bmi::tag< AliasItem::dst_type >
                                                                                                , bmi::const_mem_fun< AliasItem
                                                                                                                    , AliasItem::dst_type
                                                                                                                    , &AliasItem::dst >
                                                                                                , boost::hash< AliasItem::dst_type > > > >;
using AliasChildSet = boost::multi_index_container< AliasChildItem
                                                  , bmi::indexed_by< bmi::hashed_unique< bmi::tag< AliasChildItem::pair_type >
                                                                                                 , bmi::const_mem_fun< AliasChildItem
                                                                                                                     , AliasChildItem::pair_type
                                                                                                                     , &AliasChildItem::pair >
                                                                                                 , boost::hash< AliasChildItem::pair_type > >
                                                                   , bmi::hashed_non_unique< bmi::tag< AliasChildItem::parent_type >
                                                                                                     , bmi::const_mem_fun< AliasChildItem
                                                                                                                         , AliasChildItem::parent_type
                                                                                                                         , &AliasChildItem::parent >
                                                                                                     , boost::hash< AliasChildItem::parent_type > >
                                                                   , bmi::hashed_non_unique< bmi::tag< AliasChildItem::child_type >
                                                                                                     , bmi::const_mem_fun< AliasChildItem
                                                                                                                         , AliasChildItem::child_type
                                                                                                                         , &AliasChildItem::child >
                                                                                                     , boost::hash< AliasChildItem::child_type > > > >;

class AliasStore
{
    Kmap& km_;
    AliasSet alias_set_ = {};
    AliasChildSet alias_child_set_ = {};

public:
    AliasStore( Kmap& km );

    // Core
    auto erase_alias( Uuid const& id )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto is_alias( Uuid const& id ) const
        -> bool;
    
    // Util
    auto aliases() const
        -> AliasSet const&;
    auto child_aliases() const
        -> AliasChildSet const&;
    auto erase_aliases_to( Uuid const& id )
        -> Result< void >;
    [[ nodiscard ]]
    auto fetch_alias_children( Uuid const& parent ) const
        -> std::vector< Uuid >;
    [[ nodiscard ]]
    auto fetch_aliases_to( Uuid const& src ) const
        -> std::vector< Uuid >; // TODO: Result< UuidSet > instead?
    auto fetch_parent( Uuid const& child ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto is_alias( Uuid const& src
                 , Uuid const& dst ) const
        -> bool;
    [[ nodiscard ]]
    auto has_alias( Uuid const& node ) const
        -> bool;
    auto push_alias( Uuid const& src
                   , Uuid const& dst )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto resolve( Uuid const& id ) const
        -> Uuid;
};

} // namespace kmap

#endif // KMAP_ALIAS_STORE_HPP
