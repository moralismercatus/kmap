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
    using alias_type = Uuid;
    using src_type   = StrongType< Uuid, class SrcId >;
    using rsrc_type   = StrongType< Uuid, class RSrcId >;
    using dst_type   = StrongType< Uuid, class DstId >;

    src_type src_id{}; // TODO: Having difficulty understanding the use case for storing unresolves source here. Likewise, distinction between source and resolved source?
    rsrc_type rsrc_id{};
    dst_type dst_id{};

    auto alias() const { return make_alias_id( rsrc_id.value(), dst_id.value() ); }
    auto src() const { return src_id; }
    auto rsrc() const { return rsrc_id; }
    auto dst() const { return dst_id; }
};

struct AliasLoadItem : public AliasItem
{
    using loaded_type = bool;

    loaded_type loaded_flag = false;

    auto loaded() const{ return loaded_flag; }
};

[[ nodiscard ]]
auto make_alias_item( Uuid const& src
                    , Uuid const& rsrc
                    , Uuid const& dst )
    ->AliasItem; 

using AliasSet = boost::multi_index_container< AliasItem
                                             , bmi::indexed_by< bmi::hashed_unique< bmi::tag< AliasItem::alias_type >
                                                                                  , bmi::const_mem_fun< AliasItem
                                                                                                      , AliasItem::alias_type
                                                                                                      , &AliasItem::alias >
                                                                                  , boost::hash< AliasItem::alias_type > >
                                                              , bmi::hashed_non_unique< bmi::tag< AliasItem::src_type >
                                                                                      , bmi::const_mem_fun< AliasItem
                                                                                                          , AliasItem::src_type
                                                                                                          , &AliasItem::src >
                                                                                      , boost::hash< AliasItem::src_type > >
                                                              , bmi::hashed_non_unique< bmi::tag< AliasItem::rsrc_type >
                                                                                      , bmi::const_mem_fun< AliasItem
                                                                                                          , AliasItem::rsrc_type
                                                                                                          , &AliasItem::rsrc >
                                                                                      , boost::hash< AliasItem::rsrc_type > >
                                                              , bmi::hashed_non_unique< bmi::tag< AliasItem::dst_type >
                                                                                      , bmi::const_mem_fun< AliasItem
                                                                                                          , AliasItem::dst_type
                                                                                                          , &AliasItem::dst >
                                                                                      , boost::hash< AliasItem::dst_type > > > >;
using AliasLoadSet = boost::multi_index_container< AliasLoadItem
                                                 , bmi::indexed_by< bmi::hashed_unique< bmi::tag< AliasItem::alias_type >
                                                                                      , bmi::const_mem_fun< AliasItem
                                                                                                          , AliasItem::alias_type
                                                                                                          , &AliasItem::alias >
                                                                                      , boost::hash< AliasItem::alias_type > >
                                                                  , bmi::hashed_non_unique< bmi::tag< AliasItem::src_type >
                                                                                          , bmi::const_mem_fun< AliasItem
                                                                                                              , AliasItem::src_type
                                                                                                              , &AliasItem::src >
                                                                                          , boost::hash< AliasItem::src_type > >
                                                                  , bmi::hashed_non_unique< bmi::tag< AliasItem::rsrc_type >
                                                                                          , bmi::const_mem_fun< AliasItem
                                                                                                              , AliasItem::rsrc_type
                                                                                                              , &AliasItem::rsrc >
                                                                                          , boost::hash< AliasItem::rsrc_type > >
                                                                  , bmi::hashed_non_unique< bmi::tag< AliasItem::dst_type >
                                                                                          , bmi::const_mem_fun< AliasItem
                                                                                                              , AliasItem::dst_type
                                                                                                              , &AliasItem::dst >
                                                                                          , boost::hash< AliasItem::dst_type > >
                                                                  , bmi::ordered_non_unique< bmi::tag< AliasLoadItem::loaded_type >
                                                                                           , bmi::const_mem_fun< AliasLoadItem
                                                                                                               , AliasLoadItem::loaded_type
                                                                                                               , &AliasLoadItem::loaded > > > >;

class AliasStore
{
    AliasSet alias_set_ = {};

public:
    AliasStore() = default;

    // Core
    auto erase_alias( Uuid const& id )
        -> Result< void >;
    [[ nodiscard ]]
    auto is_alias( Uuid const& id ) const
        -> bool;
    
    // Util
    auto aliases() const
        -> AliasSet const&;
    [[ nodiscard ]]
    auto fetch_alias_children( AliasItem::alias_type const& parent ) const
        -> std::set< Uuid >;
    [[ nodiscard ]]
    auto fetch_aliases( AliasItem::rsrc_type const& rsrc ) const
        -> std::set< Uuid >;
    [[ nodiscard ]]
    auto fetch_dsts( AliasItem::rsrc_type const& rsrc ) const
        -> std::set< Uuid >;
    auto fetch_entry( AliasItem::alias_type const& child ) const
        -> Result< AliasItem >;
    auto fetch_parent( AliasItem::alias_type const& child ) const
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto is_alias( Uuid const& src
                 , Uuid const& dst ) const
        -> bool;
    [[ nodiscard ]]
    auto has_alias( Uuid const& node ) const
        -> bool;
    auto push_alias( AliasItem const& item )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto resolve( Uuid const& id ) const
        -> Uuid;
};

} // namespace kmap

#endif // KMAP_ALIAS_STORE_HPP
