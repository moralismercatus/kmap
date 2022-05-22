/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_ALIAS_HPP
#define KMAP_ALIAS_HPP

#include "common.hpp"

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/uuid/uuid_hash.hpp>

namespace bmi = boost::multi_index;

namespace kmap {

class Kmap;

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
} // namespace kmap

#endif // KMAP_ALIAS_HPP
