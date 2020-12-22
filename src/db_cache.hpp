/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_CACHE_HPP
#define KMAP_DB_CACHE_HPP

#include "common.hpp"

#include <boost/container_hash/hash.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace kmap
{

/**
 * The Idea behind this is that DB transactions are inherently slow,
 * and the vast majority of DB interactions are reads.
 * TODO: Surely there is a better caching mechansim than hand crafting. This is quite a labor-instensive way of caching that must be hard coded per table. I view this as a temporary workaround to the caching shortage.
 **/
class DbCache
{
public:
    template< typename T >
    using Identity = boost::multi_index::identity< T >;
    template< typename T >
    using OrderedUnique = boost::multi_index::ordered_unique< T >;
    template< typename T >
    using UnorderedUnique = boost::multi_index::hashed_unique< T >;
    template< typename T >
    using UnorderedNonUnique = boost::multi_index::hashed_non_unique< T >;
    template< typename T >
    using OrderedNonUnique = boost::multi_index::ordered_non_unique< T >;
    using IdStrPair = std::pair< Uuid
                               , std::string >;
    using IdIdPair = std::pair< Uuid
                              , Uuid >;
    using IndexedByUniqueId = boost::multi_index::indexed_by< UnorderedUnique< Identity< Uuid > >
                                                            , OrderedUnique< Identity< Uuid > > >;
    using IndexedByUniqueIdMultiStr = boost::multi_index::indexed_by< UnorderedUnique< Identity< IdStrPair > >
                                                                    , UnorderedUnique< boost::multi_index::member< IdStrPair
                                                                                                                 , decltype( IdStrPair::first )
                                                                                                                 , &IdStrPair::first > >
                                                                    , OrderedNonUnique< boost::multi_index::member< IdStrPair // TODO: Should this be UnorderedNonUnique?
                                                                                                                  , decltype( IdStrPair::second )
                                                                                                                  , &IdStrPair::second > > >;
    using IndexedByUniqueIdUniqueId = boost::multi_index::indexed_by< UnorderedUnique< Identity< IdIdPair > >
                                                                    , UnorderedUnique< boost::multi_index::member< IdIdPair
                                                                                                                 , decltype( IdIdPair::first )
                                                                                                                 , &IdIdPair::first > >
                                                                    , UnorderedUnique< boost::multi_index::member< IdIdPair
                                                                                                                 , decltype( IdIdPair::second )
                                                                                                                 , &IdIdPair::second > > >;
    using IndexedByUniqueIdMultiId = boost::multi_index::indexed_by< UnorderedUnique< Identity< IdIdPair > >
                                                                   , UnorderedUnique< boost::multi_index::member< IdIdPair
                                                                                                                , decltype( IdIdPair::first )
                                                                                                                , &IdIdPair::first > >
                                                                   , UnorderedNonUnique< boost::multi_index::member< IdIdPair
                                                                                                                   , decltype( IdIdPair::second )
                                                                                                                   , &IdIdPair::second > > >;
    using IndexedByNonUniqueIdNonUniqueId = boost::multi_index::indexed_by< UnorderedUnique< Identity< IdIdPair > >
                                                                          , UnorderedNonUnique< boost::multi_index::member< IdIdPair
                                                                                                                          , decltype( IdIdPair::first )
                                                                                                                          , &IdIdPair::first > >
                                                                          , UnorderedNonUnique< boost::multi_index::member< IdIdPair
                                                                                                                          , decltype( IdIdPair::second )
                                                                                                                          , &IdIdPair::second > > >;
    using UniqueIdSet = boost::multi_index_container< Uuid
                                                    , IndexedByUniqueId >;
    using UniqueIdMultiStrSet = boost::multi_index_container< IdStrPair
                                                            , IndexedByUniqueIdMultiStr >;
    using UniqueIdUniqueIdSet = boost::multi_index_container< IdIdPair
                                                            , IndexedByUniqueIdUniqueId >;
    using UniqueIdMultiIdSet = boost::multi_index_container< IdIdPair
                                                           , IndexedByUniqueIdMultiId >;
    using NonUniqueIdNonUniqueIdSet = boost::multi_index_container< IdIdPair
                                                                  , IndexedByNonUniqueIdNonUniqueId >;
    using NodeSet = UuidUnSet;
                                     
    using HeadingSet = UniqueIdMultiStrSet;
    using HeadingMap = std::unordered_map< Uuid
                                         , Heading
                                         , boost::hash< Uuid > >;
    using TitleSet = UniqueIdMultiStrSet;
    using TitleMap = std::unordered_map< Uuid
                                       , Title
                                       , boost::hash< Uuid > >;
    using BodySet = UniqueIdMultiStrSet;
    using BodyMap = std::unordered_map< Uuid
                                      , std::string
                                      , boost::hash< Uuid > >;
    using ChildSet = NonUniqueIdNonUniqueIdSet;
    using ChildMap = std::unordered_map< Uuid
                                       , NodeSet
                                       , boost::hash< Uuid > >;
    using AliasSet = NonUniqueIdNonUniqueIdSet;
    using ChildOrderingsSet = UniqueIdMultiStrSet;
    using ChildOrderingsMap = std::unordered_map< Uuid
                                                , std::string
                                                , boost::hash< Uuid > >;
    template< typename Set >
    struct CacheItem 
    {
        Set set = {};
        NodeSet completed_set = {};
        bool set_complete = false;
    };
    struct DbCacheData
    {
        CacheItem< NodeSet > nodes = {};
        CacheItem< HeadingSet > headings = {};
        CacheItem< TitleSet > titles = {};
        CacheItem< BodySet > bodies = {};
        CacheItem< ChildSet > children = {}; // Note: children either contains all children or none.
        CacheItem< AliasSet > aliases = {};
        CacheItem< ChildOrderingsSet > child_orderings = {};
        Optional< Uuid > root = {};
    };

    auto flush()
        -> void;
    auto fetch_aliases()
        -> CacheItem< AliasSet > const&;
    auto fetch_nodes() const
        -> CacheItem< NodeSet > const&;
    auto fetch_headings() const
        -> CacheItem< HeadingSet > const&;
    auto fetch_titles() const
        -> CacheItem< TitleSet > const&;
    auto fetch_bodies() const
        -> CacheItem< BodySet > const&;
    auto fetch_children() const
        -> CacheItem< ChildSet > const&;
    auto fetch_child_orderings() const
        -> CacheItem< ChildOrderingsSet > const&;
    auto insert_alias( Uuid const& src
                     , Uuid const& dst )
        -> void;
    auto insert_node( Uuid const& node )
        -> void;
    auto insert_heading( Uuid const& node
                       , Heading const& heading )
        -> void;
    auto insert_title( Uuid const& node
                     , Title const& heading )
        -> void;
    auto insert_body( Uuid const& node
                    , std::string const& body )
        -> void;
    auto insert_children( Uuid const& parent
                        , UuidSet const& children )
        -> void;
    auto insert_child_orderings( Uuid const& parent
                               , std::string const& orderings )
        -> void;
    auto set_root( Uuid const& id )
        -> Uuid;
    auto is_root( Uuid const& id ) const
        -> bool;
    auto mark_alias_complete( Uuid const& dst )
        -> void;
    auto mark_node_set_complete()
        -> void;
    auto mark_heading_set_complete()
        -> void;
    auto mark_body_set_complete()
        -> void;

private:
    DbCacheData data_ = {};
};

} // namespace kmap

#endif // KMAP_DB_HPP
