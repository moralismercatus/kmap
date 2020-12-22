
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CONTAINER_HPP
#define KMAP_CONTAINER_HPP

#include "common.hpp"

#include <boost/container_hash/hash.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <set>

namespace kmap {
    
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

using IdPair = std::pair< Uuid, Uuid >;
using IdIdsSet = boost::multi_index_container< IdPair
                                             , boost::multi_index::indexed_by< UnorderedUnique< Identity< IdPair > >
                                                                             , UnorderedNonUnique< boost::multi_index::member< IdPair
                                                                                                                             , decltype( IdPair::first )
                                                                                                                             , &IdPair::first > >
                                                                             , UnorderedNonUnique< boost::multi_index::member< IdPair
                                                                                                                             , decltype( IdPair::second )
                                                                                                                             , &IdPair::second > > > >;


} // namespace kmap

#endif // KMAP_CONTAINER_HPP