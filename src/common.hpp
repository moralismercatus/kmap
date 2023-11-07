/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COMMON_HPP
#define KMAP_COMMON_HPP

#include "error/master.hpp"
#include <util/log/log.hpp> // Make logging common to all.

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

#include <compare>
#include <ostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#define KMAP_MACRO_COMMA_WORKAROUND ,

namespace bmi = boost::multi_index;

namespace kmap {

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
inline auto const nullopt = boost::none; // To be replaced by std::nullopt if boost::optional is replaced with std::optional.
template< typename T > struct always_false : std::false_type {};

template< typename T
        , typename Tag >
class StrongType
{
public:
    using value_type = T;

    template< typename = std::enable_if< std::is_default_constructible< T >::value > >
    explicit constexpr StrongType() {}
    explicit constexpr StrongType( T const& t ) : t_{ t } {}
    explicit constexpr StrongType( T&& t ) : t_{ std::move( t ) } {}

    auto value() -> T& { return t_; }
    auto value() const -> T const& { return t_; }

    std::strong_ordering operator<=>( StrongType const& ) const = default;

private:
    T t_;
};

template< typename T
        , typename Tag >
auto hash_value( StrongType< T, Tag > const& item )
    -> std::size_t
{
    return boost::hash< T >{}( item.value() );
}

} // namespace kmap

#endif // KMAP_COMMON_HPP
