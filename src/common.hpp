/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COMMON_HPP
#define KMAP_COMMON_HPP

#include "error/master.hpp"

#include <boost/container_hash/hash.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/functional/hash.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_hash.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/outcome.hpp>

#include <ostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace bmi = boost::multi_index;

namespace kmap {

class Kmap;

using StringSet = std::set< std::string >;
using StringVec = std::vector< std::string >;
using Uuid = boost::uuids::uuid;
using Heading = std::string;
using InvertedHeading = std::string;
using Title = std::string;
using HeadingPath = StringVec; 
using UuidVec = std::vector< Uuid >;
using UuidPath = UuidVec;
using UuidPair = std::pair< Uuid, Uuid >;
using UuidSet = std::set< Uuid >;
using UuidUnSet = std::unordered_set< Uuid
                                    , boost::hash< Uuid > >;
template< typename T > using Optional = boost::optional< T >;
using FsPath = boost::filesystem::path;
using IdStrPair = std::pair< Uuid
                           , std::string >;
using IdHeadingSet = bmi::multi_index_container< IdStrPair
                                               , bmi::indexed_by< bmi::hashed_unique< bmi::tag< decltype( IdStrPair::first ) >
                                                                                    , bmi::member< IdStrPair
                                                                                                 , decltype( IdStrPair::first )
                                                                                                 , &IdStrPair::first > >
                                                                , bmi::hashed_non_unique< bmi::tag< decltype( IdStrPair::second ) >
                                                                                        , bmi::member< IdStrPair
                                                                                                     , decltype( IdStrPair::second )
                                                                                                     , &IdStrPair::second > > > >;
template< typename T > using Result = error_code::Result< T >;
auto const kmap_root_dir = FsPath{ "/kmap" };
inline auto const nullopt = boost::none; // To be replaced by std::nullopt if boost::optional is replaced with std::optional.
template< typename T > struct always_false : std::false_type {};

auto root_dir()
    -> std::string;

    
} // namespace kmap

#endif // KMAP_COMMON_HPP
