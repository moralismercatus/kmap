/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_COMMON_HPP
#define KMAP_DB_COMMON_HPP

#include "../common.hpp"
#include "../utility.hpp"
#include "error/db.hpp"

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <fmt/format.h>

#include <concepts>
#include <tuple>
#include <variant>

namespace kmap::db {

class Cache;

using Left = StrongType< Uuid, class LeftTag >;
using Right = StrongType< Uuid, class RightTag >;
using LRPair = std::pair< Left, Right >;
using UuidPair = std::pair< Uuid, Uuid >; // TODO: I think UuidPair needs to be rather LRPair (std::pair< Left, Right >)
// using KeyVariant = std::variant< Uuid, UuidPair, Left, Right >;
using UniqueKeyVariant = std::variant< Uuid, Left, Right, UuidPair, LRPair >;

using Parent = Left;
using Child = Right;
using Src = Left;
using Dst = Right;

enum class DeltaType
{
    created
,   changed
,   erased
};

using TransactionId = Uuid;
using TransactionIdSet = std::set< Uuid >;

template< typename Value >
struct DeltaItem
{
    Value value = {};
    DeltaType action = {};
    TransactionId transaction_id = {};

    std::strong_ordering operator<=>( DeltaItem const& ) const = default;
};

template< typename Value >
using DeltaItems = std::vector< DeltaItem< Value > >;

template< typename Left
        , typename Right >
struct TableItemLR
{
    using key_type = std::pair< Left, Right >;

    std::optional< key_type > cache_item = {};
    DeltaItems< key_type > delta_items = {};

    auto key() const
    { 
        if( delta_items.empty() ) 
        {
            KMAP_ENSURE_EXCEPT( cache_item );

            return cache_item.value();
        }
        else
        {
            return delta_items.back().value;
        }
    }
    auto left() const
    { 
        return key().first;
    }
    auto right() const
    { 
        return key().second;
    }

    std::strong_ordering operator<=>( TableItemLR const& ) const = default;
};

template< typename L
        , typename R
        , typename Value >
auto get( TableItemLR< L, R > const& ti )
{
    return std::get< Value >( ti.key() );
}

template< typename Left
        , typename Right >
struct TableItemL
{
    using key_type = Left;

    TableItemL( key_type const& k
              , std::optional< Right > const& citem
              , DeltaItems< Right > const& ditems )
        : lkey{ k }
        , cache_item{ citem }
        , delta_items{ ditems }
    {
    }

    Left lkey;
    std::optional< Right > cache_item = {};
    DeltaItems< Right > delta_items = {};

    auto key() const
    { 
        return lkey;
    }
    auto left() const
    { 
        return key();
    }
    auto right() const
    { 
        if( delta_items.empty() ) 
        {
            KMAP_ENSURE_EXCEPT( cache_item ); // TODO: couldn't this be enforced by ctor?

            return cache_item.value();
        }
        else
        {
            KMAP_ENSURE_EXCEPT( !delta_items.empty() ); // TODO: couldn't this be enforced by ctor?

            return delta_items.back().value;
        }
    }

    std::strong_ordering operator<=>( TableItemL const& ) const = default;
};

template< typename Key >
struct TableItemMono
{
    using key_type = Key;

    Key ukey;
    std::optional< Key > cache_item = {};
    DeltaItems< Key > delta_items = {}; // TODO: Since DeltaItem::value isn't used, we can actually supply an empty struct e.g., DeltaItems< EmptyType > delta_items_ = {};

    auto key() const
    { 
        return ukey;
    }
    auto left() const
    { 
        return key();
    }
    auto right() const
    { 
        return key();
    }

    std::strong_ordering operator<=>( TableItemMono const& ) const = default;
};

struct right_ordered {};

// template< typename Left
//         , typename Right >
// using TableItemKV = TableItem< std::pair< Left, Right > >;
template< typename Left
        , typename Right >
using UniqueLRTable = boost::multi_index_container< TableItemLR< Left, Right >
                                                  , bmi::indexed_by< bmi::hashed_unique< bmi::tag< typename TableItemLR< Left, Right >::key_type >
                                                                                                 , bmi::const_mem_fun< TableItemLR< Left, Right >
                                                                                                                     , typename TableItemLR< Left, Right >::key_type 
                                                                                                                     , &TableItemLR< Left, Right >::key >
                                                                                                 , boost::hash< typename TableItemLR< Left, Right >::key_type  > >
                                                                   , bmi::hashed_non_unique< bmi::tag< Left >
                                                                                           , bmi::const_mem_fun< TableItemLR< Left, Right >
                                                                                                               , Left 
                                                                                                               , &TableItemLR< Left, Right >::left >
                                                                                           , boost::hash< Left > >
                                                                   , bmi::hashed_non_unique< bmi::tag< Right >
                                                                                           , bmi::const_mem_fun< TableItemLR< Left, Right >
                                                                                                               , Right
                                                                                                               , &TableItemLR< Left, Right >::right >
                                                                                           , boost::hash< Right > > > >;

template< typename Left
        , typename Right >
using UniqueLhsTable = boost::multi_index_container< TableItemL< Left, Right >
                                                   , bmi::indexed_by< bmi::hashed_unique< bmi::tag< Left >
                                                                                        , bmi::const_mem_fun< TableItemL< Left, Right >
                                                                                                            , Left 
                                                                                                            , &TableItemL< Left, Right >::left >
                                                                                        , boost::hash< Left > >
                                                                    , bmi::hashed_non_unique< bmi::tag< Right >
                                                                                            , bmi::const_mem_fun< TableItemL< Left, Right >
                                                                                                                , Right
                                                                                                                , &TableItemL< Left, Right >::right >
                                                                                            , boost::hash< Right > > 
                                                                    , bmi::ordered_non_unique< bmi::tag< right_ordered >
                                                                                             , bmi::const_mem_fun< TableItemL< Left, Right >
                                                                                                                 , Right
                                                                                                                 , &TableItemL< Left, Right >::right > > > >;

template< typename KV >
using UniqueMonoTable = boost::multi_index_container< TableItemMono< KV >
                                                    , bmi::indexed_by< bmi::hashed_unique< bmi::tag< KV >
                                                                                         , bmi::const_mem_fun< TableItemMono< KV >
                                                                                                             , KV
                                                                                                             , &TableItemMono< KV >::key >
                                                                                         , boost::hash< KV > > > >;

template< typename KV >
class SetTable
{
    UniqueMonoTable< KV > table;

public:
    using key_type = KV;
    using left_type = KV;
    using right_type = KV;
    using value_type = KV;
    using table_item_type = TableItemMono< KV >;
    using unique_key_type = KV;
    using Set = std::set< KV >;
    using DeltaItems = decltype( TableItemMono< KV >::delta_items );
    using DeltaItem = typename DeltaItems::value_type;

    auto contains( KV const& kv ) const
        -> bool
    {
        auto const& tv = table.template get< KV >();

        return tv.find( kv ) != tv.end();
    }
    auto fetch( KV const& kv ) const
        -> Result< table_item_type >
    {
        auto rv = KMAP_MAKE_RESULT( table_item_type );

        if( auto const it = table.find( kv )
          ; it != table.end() )
        {
            rv = *it;
        }

        return rv;
    }
    auto create( KV const& kv )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        // fmt::print( "creating set entry: {}\n", to_string( kv ) );
        if( !contains( kv ) )
        {
            auto const di = DeltaItem{ .value = kv 
                                     , .action = DeltaType::created
                                     , .transaction_id = {}/*TODO*/ };

            if( auto const [ it, success ] = table.emplace( table_item_type{ .ukey = kv, .cache_item = {}, .delta_items = DeltaItems{ di } } )
              ; success )
            {
                rv = outcome::success();
            }
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::common::data_already_exists );
        }

        return rv;
    }
    auto erase( KV const& p )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        auto&& tv = table.template get< KV >();

        if( auto const it = tv.find( p )
          ; it != tv.end() )
        {
            tv.erase( it );

            rv = outcome::success();
        }

        return rv;
    }
    template< typename UpdateFn >
    auto update( KV const& kv 
               , UpdateFn const& update_fn )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );
        auto const& tv = table.template get< KV >();
        
        // fmt::print( "updating settable entry: ( 'ukey' )\n" );
        if( auto const it = tv.find( kv )
          ; it != tv.end() )
        {
            if( table.modify( it, update_fn ) )
            {
                rv = outcome::success();
            }
            else
            {
                rv = KMAP_MAKE_ERROR( error_code::db::update_failed );
            }
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::db::entry_not_found );
        }

        return rv;
    }

    auto underlying() const
        -> UniqueMonoTable< KV > const&
    {
        return table;
    }

    auto begin() const { return table.begin(); }
    auto end() const { return table.end(); }
};

template< typename Key
        , typename Value >
class MapTable
{
    UniqueLhsTable< Key, Value > table;

public:
    using left_type = Key;
    using right_type = Value;
    using key_type = Key;
    using value_type = Value;
    using table_item_type = TableItemL< Key, Value >;
    using unique_key_type = Key;
    using DeltaItems = decltype( TableItemL< Key, Value >::delta_items );
    using DeltaItem = typename DeltaItems::value_type;

    auto contains( left_type const& left ) const 
        -> bool
    {
        auto const& tv = table.template get< left_type >();

        return tv.find( left ) != tv.end();
    }
    auto contains( right_type const& right ) const 
        -> bool
    {
        auto const& tv = table.template get< Right >();

        return tv.find( right ) != tv.end();
    }
    auto fetch( left_type const& left ) const 
        -> Result< table_item_type >
    {
        auto rv = KMAP_MAKE_RESULT( table_item_type );

        if( auto const it = table.find( left )
          ; it != table.end() )
        {
            rv = *it;
        }

        return rv;
    }
    auto fetch( right_type const& right ) const 
        -> Result< std::vector< table_item_type > >
    {
        auto rv = KMAP_MAKE_RESULT( std::vector< table_item_type > );
        auto er = std::vector< table_item_type >{};
        auto const& tv = table.template get< right_type >();

        for( auto [ it, end ] = tv.equal_range( right )
           ; it != end
           ; ++it )
        {
            er.emplace_back( *it );
        }

        if( !er.empty() )
        {
            rv = er;
        }

        return rv;
    }
    auto create( Key const& key
               , Value const& val )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        // fmt::print( "creating maptable entry: ( '{}', '{}' )\n", to_string( key ), val );
        if( !contains( key ) )
        {
            auto const di = DeltaItem{ .value = val
                                     , .action = DeltaType::created
                                     , .transaction_id = {}/*TODO*/ };

            if( auto const [ it, success ] = table.insert( table_item_type{ key, std::nullopt, DeltaItems{ di } } )
              ; success )
            {
                rv = outcome::success();
            }
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::common::data_already_exists );
        }

        return rv;
    }
    auto erase( unique_key_type const& p )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        auto&& tv = table.template get< unique_key_type >();

        if( auto const it = tv.find( p )
          ; it != tv.end() )
        {
            tv.erase( it );

            rv = outcome::success();
        }

        return rv;
    }
    template< typename UpdateFn >
    auto update( left_type const& left
               , UpdateFn const& update_fn )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );
        auto const& tv = table.template get< left_type >();
        
        // fmt::print( "updating maptable entry: ( '{}' )\n", to_string( left ) );
        if( auto const it = tv.find( left )
          ; it != tv.end() )
        {
            if( table.modify( it, update_fn ) )
            {
                rv = outcome::success();
            }
        }

        return rv;
    }

    auto underlying() const
        -> UniqueLhsTable< Key, Value > const&
    {
        return table;
    }

    auto begin() const { return table.begin(); }
    auto end() const { return table.end(); }
};

template< typename Left
        , typename Right >
class BimapTable
{
    UniqueLRTable< Left, Right > table;

public:
    using left_type = Left;
    using right_type = Right;
    using key_type = std::pair< Left, Right >;
    using value_type = std::pair< Left, Right >;
    using table_item_type = TableItemLR< Left, Right >;
    using unique_key_type = std::pair< Left, Right >;
    using Pair = unique_key_type;
    using DeltaItems = decltype( TableItemLR< Left, Right >::delta_items );
    using DeltaItem = typename DeltaItems::value_type;

    // Hack for what should be a TPM `value_type< T > = left_type ? right_type : left_type` construct.
    // For now, use decltype( value_type( L|R ) )
    static constexpr auto value_type_t( left_type ) { return right_type{}; }
    static constexpr auto value_type_t( right_type ) { return left_type{}; }
    template< typename T >
    using value_type_v = decltype( value_type_t( T{} ) );

    auto contains( Left const& left ) const
        -> bool
    {
        auto const& tv = table.template get< Left >();

        return tv.find( left ) != tv.end();
    }
    auto contains( Right const& right ) const
        -> bool
    {
        auto const& tv = table.template get< Right >();

        return tv.find( right ) != tv.end();
    }
    auto contains( Pair const& p ) const
        -> bool
    {
        return table.find( p ) != table.end();
    }
    auto contains( Left const& left
                 , Right const& right ) const
        -> bool
    {
        return table.find( Pair{ left, right } ) != table.end();
    }
    auto fetch( Left const& left ) const
        -> Result< std::vector< table_item_type > >
    {
        auto rv = KMAP_MAKE_RESULT( std::vector< table_item_type > );
        auto&& tv = table.template get< Left >();
        auto er = std::vector< table_item_type >{};

        for( auto [ it, end ] = tv.equal_range( left )
           ; it != end
           ; ++it )
        {
            er.emplace_back( *it );
        }

        if( !er.empty() )
        {
            rv = er;
        }

        return rv;
    }
    auto fetch( Right const& right ) const
        -> Result< std::vector< table_item_type > >
    {
        auto rv = KMAP_MAKE_RESULT( std::vector< table_item_type > );
        auto er = std::vector< table_item_type >{};
        auto const& tv = table.template get< Right >();

        for( auto [ it, end ] = tv.equal_range( right )
           ; it != end
           ; ++it )
        {
            er.emplace_back( *it );
        }

        if( !er.empty() )
        {
            rv = er;
        }

        return rv;
    }
    auto fetch( Pair const& p ) const
        -> Result< table_item_type >
    {
        auto rv = KMAP_MAKE_RESULT( table_item_type );
        auto&& tv = table.template get< Pair >();

        if( auto const it = tv.find( p )
          ; it != tv.end() )
        {
            rv = *it;
        }

        return rv;
    }
    auto create( Left const& left
               , Right const& right )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        // fmt::print( "creating bimaptable entry: ( '{}', '{}' )\n", to_string( left.value() ), to_string( right.value() ) );
        if( !contains( { left, right } ) )
        {
            auto const di = DeltaItem{ .value = { left, right }
                                     , .action = DeltaType::created
                                     , .transaction_id = {}/*TODO*/ };

            if( auto const [ it, success ] = table.emplace( table_item_type{ .cache_item = {}, .delta_items = DeltaItems{ di } } )
              ; success )
            {
                rv = outcome::success();
            }
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::common::data_already_exists );
        }

        return rv;
    }
    auto erase( unique_key_type const& p )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        auto&& tv = table.template get< unique_key_type >();

        if( auto const it = tv.find( p )
          ; it != tv.end() )
        {
            tv.erase( it );

            rv = outcome::success();
        }

        return rv;
    }
    template< typename UpdateFn >
    auto update( unique_key_type const& ukey 
               , UpdateFn const& update_fn )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );
        auto const& tv = table.template get< unique_key_type >();
        
        // fmt::print( "updating bimaptable entry: ( 'ukey' )\n" );
        if( auto const it = tv.find( ukey )
          ; it != tv.end() )
        {
            if( table.modify( it, update_fn ) )
            {
                rv = outcome::success();
            }
        }

        return rv;
    }

    auto underlying() const
        -> UniqueLRTable< Left, Right > const&
    {
        return table;
    }

    auto begin() const { return table.begin(); }
    auto end() const { return table.end(); }
};

// template< typename T >
// concept MultiValueTable = requires( T v )
// {
//     { v.fetch( T::key_type ) } = 
// };

struct NodeTable : public SetTable< Uuid > {};
struct HeadingTable : public MapTable< Uuid, std::string > {};
struct TitleTable : public MapTable< Uuid, std::string > {};
struct BodyTable : public MapTable< Uuid, std::string > {};
struct ChildTable : public BimapTable< Parent, Child > {};
struct AliasTable : public BimapTable< Src, Dst > {};
struct AttributeTable : public BimapTable< Src, Dst > {};
struct ResourceTable : public MapTable< Uuid, std::vector< std::byte > > {};

using TableVariant = std::variant< NodeTable
                                 , HeadingTable
                                 , TitleTable
                                 , BodyTable
                                 , ChildTable
                                 , AliasTable
                                 , AttributeTable
                                 , ResourceTable >;
using ValueVariant = std::variant< std::string
                                 , Uuid
                                 , Left
                                 , Right
                                 , std::pair< Uuid, std::string >
                                 , std::pair< Uuid, Uuid >
                                 , std::pair< Left, Right >
                                 , std::pair< Uuid, std::vector< std::byte > >
                                 , std::vector< std::byte >
                                 , std::vector< Right > >;

[[ nodiscard ]]
auto contains_cached( Cache const& cache
                    , TableVariant const& table
                    , UniqueKeyVariant const& key )
    -> bool;
[[ nodiscard ]]
auto contains_delta( Cache const& cache
                   , TableVariant const& table
                   , UniqueKeyVariant const& key )
    -> bool;
[[ nodiscard ]]
auto fetch_cached( Cache const& cache
                 , TableVariant const& table
                 , UniqueKeyVariant const& key )
    -> Result< ValueVariant >;
[[ nodiscard ]]
auto fetch_deltas( Cache const& cache
                 , TableVariant const& table
                 , UniqueKeyVariant const& key )
    -> Result< DeltaItems< ValueVariant > >;

} // namespace kmap::db

namespace fmt {

template <>
struct formatter< kmap::db::UniqueKeyVariant >
{
    constexpr auto parse( format_parse_context& ctx )
    {
        // assert( false && "unimplemented" );

        // TODO: I believe I want ctx.begin() here, not end().
        return ctx.end();
    }

    template< typename FormatContext >
    auto format( kmap::db::UniqueKeyVariant const& key
               , FormatContext& ctx )
    {
        auto fstr = std::string{};

        std::visit( [ & ]( auto const& arg )
                    {
                        using T = std::decay_t< decltype( arg ) >;

                        if constexpr( std::is_same_v< T, kmap::Uuid > )
                        {
                            fstr = kmap::to_string( arg );
                        }
                        else if constexpr( std::is_same_v< T, kmap::db::LRPair > )
                        {
                            fstr = fmt::format( "('{}':'{}')", arg.first.value(), arg.second.value() );
                        }
                        else if constexpr( std::is_same_v< T, kmap::db::UuidPair > )
                        {
                            fstr = fmt::format( "{}", arg );
                        }
                        else if constexpr( std::is_same_v< T, kmap::db::Left > )
                        {
                            fstr = fmt::format( "{}", arg.value() );
                        }
                        else if constexpr( std::is_same_v< T, kmap::db::Right > )
                        {
                            fstr = fmt::format( "{}", arg.value() );
                        }
                        else
                        {
                            static_assert( kmap::always_false< T >::value, "non-exhaustive visitor!" );
                        }
                    }
                  , key );

        return format_to( ctx.out()
                        , fstr );
    }
};

} // namespace fmt

#endif // KMAP_DB_COMMON_HPP
