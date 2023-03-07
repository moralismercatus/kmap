/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cache.hpp"

#include "common.hpp"
#include "contract.hpp"
#include "com/database/sm.hpp"
#include "error/db.hpp"
#include "io.hpp"

#include <boost/hana/for_each.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

#include <tuple>
#include <variant>

using namespace ranges;

namespace kmap::com::db {

// auto NodesTable::contains( Uuid const& key ) const
//     -> bool
// {
//     return table.contains( key );
// }

// auto NodesTable::fetch( Uuid const& key ) const
//     -> Result< Uuid >
// {
//     auto rv = KMAP_MAKE_RESULT( Uuid );

//     if( auto const it = table.find( key )
//       ; it != table.end() )
//     {
//         rv = *it;
//     }

//     return rv;
// }

// auto NodesTable::push( Uuid const& key )
//     -> Result< void >
// {
//     auto rv = KMAP_MAKE_RESULT( void );

//     if( table.emplace( key ).second )
//     {
//         rv = outcome::success();
//     }

//     return rv;
// }

// auto HeadingsTable::contains( Uuid const& key ) const
//     -> bool
// {
//     return table.contains( key );
// }

// auto HeadingsTable::fetch( Uuid const& key ) const
//     -> Result< std::string >
// {
//     auto rv = KMAP_MAKE_RESULT( std::string );

//     if( auto const it = table.find( key )
//       ; it != table.end() )
//     {
//         rv = it->second;
//     }

//     return rv;
// }

// auto HeadingsTable::push( Uuid const& key
//                         , std::string const& val )
//     -> Result< void >
// {
//     auto rv = KMAP_MAKE_RESULT( void );

//     if( table.emplace( key, val ).second )
//     {
//         rv = outcome::success();
//     }

//     return rv;
// }

// auto ChildrenTable::contains( Child const& key ) const
//     -> bool
// {
//     return table.right.find( key ) != table.right.end();
// }

// auto ChildrenTable::contains( Parent const& key ) const
//     -> bool
// {
//     return table.left.find( key ) != table.left.end();
// }

// auto ChildrenTable::contains( UuidPair const& key ) const
//     -> bool
// {
//     return table.find( key ) != table.end();
// }

// auto ChildrenTable::fetch( Parent const& p ) const
//     -> Result< UuidPair >
// {
//     auto rv = KMAP_MAKE_RESULT( UuidPair );

//     if( auto const it = table.left.find( p.value() )
//       ; it != table.left.end() )
//     {
//         rv = std::pair{ it->first, it->second };
//     }

//     return rv;
// }

// auto ChildrenTable::fetch( Child const& c ) const
//     -> Result< UuidPair >
// {
//     auto rv = KMAP_MAKE_RESULT( UuidPair );

//     if( auto const it = table.right.find( c.value() )
//       ; it != table.right.end() )
//     {
//         rv = std::pair{ it->first, it->second };
//     }

//     return rv;
// }

// This assumes we're loading the entire DB into cache every time we open it.
// Effectively disallows multi-session to the DB, as any external modifications wouldn't be reflected in the cache.
// Perhaps a way to lessen this requirement is to store the last write time of the DB. If it doesn't match the stored one,
// We can know that the DB has been modified by another session. I'm a little leary of this approach, as there will be room for race conditions.
// There must be a way to lock a file in the FS, such as to serve as a mutex, to avail such race conditions (e.g., POSIX flock).
// Of course, in a way, this defeats the point of a cache, if the file on disk must be queried each DB query, to check if it has been modified, in a way, but not the same.
// The safest thing to do, at this point, is to lock the file as read only to other processes. This way, others can browse but not modify. Only the holder of the
// lock can modify. I think this solves problems all around, at least for the current use cases.
// This behavior would seem to be available via fcntl() and FWRLCK.
// auto Cache::push( TableId const& tid
//                 , Key const& key
//                 , CacheValue const& val )
//     -> Result< void >
// {
//     using boost::sml::state;
//     using namespace db::sm::state;

//     auto rv = KMAP_MAKE_RESULT( void );
//     auto [ decider, output ] = db::make_unique_cache_decider( *this );
    
//     while( !output->done )
//     {
//         decider->process_event( db::sm::ev::Push{ .tid=tid, .key=key, .value=val } );
//     }

//     if( decider->is( state< Nop > ) )
//     {
//         // Do nothing.
//     }
//     else if( decider->is( state< CreateDelta > ) )
//     {
//         KMAP_TRY( push_delta( tid, key, val, DeltaType::created, curr_groups_ ) );
//     }
//     else if( decider->is( state< UpdateDelta > ) )
//     {
//         KMAP_TRY( push_delta( tid, key, val, DeltaType::changed, curr_groups_ ) );
//     }
//     else // Includes state< Error >
//     {
//         rv = KMAP_MAKE_ERROR_MSG( error_code::db::push, output->error_msg );
//     }

//     rv = outcome::success();

//     return rv;
// }

// auto Cache::push_delta( TableId const& tid
//                       , Key const& key
//                       , CacheValue const& cv
//                       , DeltaType const& dt
//                       , TransactionIdSet const& groups )
//     -> Result< void >
// {
//     auto rv = KMAP_MAKE_RESULT( void );

//     if( auto const tbl_it = delta_.find( tid )
//       ; tbl_it != delta_.end() )
//     {
//         if( auto const kit = tbl_it->second.find( key )
//           ; kit != tbl_it->second.end() )
//         {
//             kit->second.emplace_back( DeltaItem{ .key = key, .action = dt, .value = cv, .groups = groups } );

//             rv = outcome::success();
//         }
//     }

//     return rv;
// }

// auto Cache::fetch_cached( TableId const& tid
//                         , Key const& key ) const
//     -> Result< CacheValue >
// {
//     auto rv = KMAP_MAKE_RESULT( CacheValue );

//     auto const dt = KMAP_TRY( fetch_cache_table( tid ) );

//     if( auto const it = dt.find( key )
//       ; it != dt.end() )
//     {
//         rv = it->second;
//     }

//     return rv;
// }

// auto Cache::fetch_cache_table( TableId const& tid ) const
//     -> Result< CacheRowMap >
// {
//     auto rv = KMAP_MAKE_RESULT( CacheRowMap );

//     if( auto const it = cache_.find( tid )
//       ; it != cache_.end() )
//     {
//         rv = it->second;
//     }

//     return rv;
// }

// auto Cache::fetch_delta( TableId const& tid
//                        , Key const& key ) const
//     -> Result< DeltaSequence >
// {
//     auto rv = KMAP_MAKE_RESULT( DeltaSequence );

//     auto const dt = KMAP_TRY( fetch_delta_table( tid ) );

//     if( auto const it = dt.find( key )
//       ; it != dt.end() )
//     {
//         rv = it->second;
//     }

//     return rv;
// }

// auto Cache::fetch_delta_table( TableId const& tid ) const
//     -> Result< DeltaRowMap >
// {
//     auto rv = KMAP_MAKE_RESULT( DeltaRowMap );

//     if( auto const it = delta_.find( tid )
//       ; it != delta_.end() )
//     {
//         rv = it->second;
//     }

//     return rv;
// }

/**
 * @brief Poor-man's pattern matching + assignment. Chainable: match_key<...>(...) || match_key<...>(...);
 */
template< typename Table
        , typename Key >
    requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
auto match_cache_key( Cache const& cache
                    , Key const& key
                    , auto&& result )
    -> bool
{
    auto found = false;

    if( auto const ci = cache.fetch_cached_value< Table >( key )
      ; ci )
    {
        result = ci.value();
        found = true;
    }

    return found;
};

/**
 * @brief Poor-man's pattern matching + assignment. Chainable: match_key<...>(...) || match_key<...>(...);
 */
template< typename Table
        , typename Key >
    requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
auto match_cache_key( Cache const& cache
                    , Key const& key
                    , auto&& result )
    -> bool
{
    auto found = false;

    if( auto const ci = cache.fetch_cached_values< Table >( key )
      ; ci )
    {
        result = ci.value();
        found = true;
    }

    return found;
};

/**
 * @brief Poor-man's pattern matching + assignment. Chainable: match_key<...>(...) || match_key<...>(...);
 */
template< typename Table
        , typename Key >
    requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
auto match_delta_key( Cache const& cache
                    , Key const& key
                    , auto&& result )
    -> bool
{
    auto found = false;

    if( auto const ti = cache.fetch_deltas< Table >( key )
      ; ti )
    {
        if( !ti.value().empty() )
        {
            result = ti.value()
                    | views::transform( []( auto const& e ){ return DeltaItem< ValueVariant >{ .value=e.value, .action=e.action, .transaction_id=e.transaction_id }; } )
                    | ranges::to< DeltaItems< ValueVariant > >();

            found = true;
        }
    }

    return found;
};

/**
 * @brief Poor-man's pattern matching + assignment. Chainable: match_key<...>(...) || match_key<...>(...);
 */
template< typename Table
        , typename Key >
    requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; } 
auto match_delta_key( Cache const& cache
                    , Key const& key
                    , auto&& result )
    -> bool
{
    auto found = false;
    auto tr = DeltaItems< ValueVariant >{};

    if( auto const ti = cache.fetch_deltas< Table >( key )
      ; ti )
    {
        if( !ti.value().empty() )
        {
            auto const& disv = ti.value();

            for( auto const& dis : disv )
            {
                auto const di = dis
                              | views::transform( []( auto const& e ){ return DeltaItem< ValueVariant >{ .value=e.value, .action=e.action, .transaction_id=e.transaction_id }; } )
                              | ranges::to< DeltaItems< ValueVariant > >();

                tr.insert( tr.begin(), di.begin(), di.end() );
            }

            result = tr;

            found = true;
        }
    }

    return found;
};

// auto Cache::delete_item( TableId const& tid
//                        , KeyVariant const& key )
//     -> Result< void >
// {
//     auto rv = KMAP_MAKE_RESULT( void );

    // if( tid == TableId::nodes )
    // {
    //     // TODO: Is there a mistake? I think so. Some tables can have duplicate keys, such as "children" and "aliases".
    //     //       I'm not particularly fond of the idea of duplicate keys, in general, but it would jive with current implementations.
    //     //       I also understand the reasoning: Typically, we go from parent -> children.
    //     //       Here are some options:
    //     //       1. Use multisets. 
    //     //           1. As most tables are sets, I can either make all multiset, or variant< set, multiset >. 
    //     //              I am more comfortable with the variant, as it enforces some invariants (no pun intended) on the tables.
    //     //       1. Change implementations.
    //     //       1. boost::bimap< key, DeltaItem( with operator==uuid defined) >
    //     for( auto&& [ tid, tbl ] : cache_ )
    //     {
    //         tbl.erase( key );
    //         // TODO: delta...
    //         // TODO: undo...
    //     }
    // }
    // else
    // {
    //     // auto const tbl = fetch( tid );
    //     // TODO: delta...
    //     // TODO: undo...
    // }

//     return rv;
// }

// auto Cache::delta() const
//     -> DeltaTableMap const&
// {
//     return delta_;
// }

auto contains_cached( Cache const& cache
                    , TableVariant const& table
                    , UniqueKeyVariant const& key )
    -> bool
{
    auto rv = false;

    if( auto const r = fetch_cached( cache, table, key )
      ; r )
    {
        rv = true;
    }

    return rv;
}

auto contains_delta( Cache const& cache
                   , TableVariant const& table
                   , UniqueKeyVariant const& key )
    -> bool
{
    auto rv = false;

    // if constexpr( concepts::Pair< Table::unique_key_type > )
    // {
    //     fetch_deltas( cache, table, { key, value } );
    // }
    // else
    // {
    //     fetch_deltas( cache, table, key );
    // }
    if( auto const r = fetch_deltas( cache, table, key )
      ; r )
    {
        rv = !r.value().empty();
    }

    return rv;
}

auto fetch_cached( Cache const& cache
                 , TableVariant const& table
                 , UniqueKeyVariant const& key )
    -> Result< ValueVariant >
{
    auto rv = KMAP_MAKE_RESULT( ValueVariant );

    std::visit( [ & ]( auto const& tbl ) mutable
                {
                    using Table = std::decay_t< decltype( tbl ) >;

                    auto const resolve_key = []( auto const& key )
                    {
                        if( std::holds_alternative< typename Table::unique_key_type >( key ) )
                        {
                            return std::get< typename Table::unique_key_type >( key );
                        }
                        else
                        {
                            KMAP_THROW_EXCEPTION_MSG( "invalid key type variant for table" );
                        }
                    };
                    auto const resolved_key = resolve_key( key );

                    if constexpr( std::is_same_v< Table, NodeTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, HeadingTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, TitleTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, BodyTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, ChildTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, AliasTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, AttributeTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, ResourceTable > )
                    {
                        match_cache_key< Table >( cache, resolved_key, rv );
                    }
                    else
                    {
                        static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
                    }
                }
                , table );

    return rv;
}

/**
 * @note This is where strong typing breaks down. The fact that we go from < Table, Key > to < TableVariant, KeyVariant >,
 *       and std::variant is a runtime type, the strong typing must become "weak" typing.
 *       The reason for using variant types is that I couldn't figure out a way for Boost.SML to work template types.
 *       Not a fan. Relies on runtime testing to verify what should be detectable at compile time.
 * @note This template => variant => template also has the downside of being very verbose (especially lacking pattern matching syntax). Not a fan.
 */
auto fetch_deltas( Cache const& cache
                 , TableVariant const& table
                 , UniqueKeyVariant const& key )
    -> Result< DeltaItems< ValueVariant > >
{
    auto rv = KMAP_MAKE_RESULT( DeltaItems< ValueVariant > );

    std::visit( [ & ]( auto const& tbl ) mutable
                {
                    using Table = std::decay_t< decltype( tbl ) >;

                    auto const resolve_key = []( auto const& key )
                    {
                        if( key.index() == std::variant_npos )
                        {
                            fmt::print( "variant does not contain a value!\n" );
                        }
                        if( std::holds_alternative< typename Table::unique_key_type >( key ) )
                        {
                            return std::get< typename Table::unique_key_type >( key );
                        }
                        else
                        {
                            KMAP_THROW_EXCEPTION_MSG( "invalid key type variant for table" );
                        }
                    };
                    auto const resolved_key = resolve_key( key );

                    if constexpr( std::is_same_v< Table, NodeTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, HeadingTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, TitleTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, BodyTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, ChildTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, AliasTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, AttributeTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, ResourceTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else
                    {
                        static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
                    }
                }
                , table );

    return rv;
}

auto print_deltas( db::Cache const& cache )
    -> void
{
    fmt::print( "Delta items:\n" );

    auto proc_table = [ & ]( auto const& table )
    {
        using namespace db;
        using Table = std::decay_t< decltype( table ) >;

        for( auto const& item : table )
        {
            if( item.delta_items.empty() )
            {
                continue;
            }

            auto const& di = item.delta_items.back();

            if constexpr( std::is_same_v< Table, NodeTable > )
            {
                fmt::print( "node delta: {}\n", to_string( item.key() ) );
            }
            else if constexpr( std::is_same_v< Table, HeadingTable > )
            {
                fmt::print( "heading delta: {}\n", di.value );
            }
            else if constexpr( std::is_same_v< Table, TitleTable > )
            {
                fmt::print( "title delta: {}\n", di.value );
            }
            else if constexpr( std::is_same_v< Table, BodyTable > )
            {
                fmt::print( "body delta: {}\n", di.value );
            }
            else if constexpr( std::is_same_v< Table, ChildTable > )
            {
                fmt::print( "child delta: ({}, {})\n", to_string( di.value.first.value() ), to_string( di.value.second.value() ) );
            }
            else if constexpr( std::is_same_v< Table, AliasTable > )
            {
                fmt::print( "alias delta: ({}, {})\n", to_string( di.value.first.value() ), to_string( di.value.second.value() ) );
            }
            else if constexpr( std::is_same_v< Table, AttributeTable > )
            {
                fmt::print( "attribute delta: ({}, {})\n", to_string( di.value.first.value() ), to_string( di.value.second.value() ) );
            }
            else if constexpr( std::is_same_v< Table, ResourceTable > )
            {
                assert( false && "TODO" );
                KMAP_THROW_EXCEPTION_MSG( "TODO" );
            }
            else
            {
                static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
            }
        }
    };

    boost::hana::for_each( cache.tables(), proc_table );
}

#if 0
auto create_delta( Cache const& cache
                 , TableVariant const& table
                 , UniqueKeyVariant const& key )
    -> Result< DeltaItems< ValueVariant > >
{
    auto rv = KMAP_MAKE_RESULT( DeltaItems< ValueVariant > );

    std::visit( [ & ]( auto const& tbl ) mutable
                {
                    using Table = std::decay_t< decltype( tbl ) >;

                    auto const resolve_key = []( auto const& key )
                    {
                        if( std::holds_alternative< typename Table::left_type >( key ) )
                        {
                            return std::get< typename Table::left_type >( key );
                        }
                        else
                        {
                            KMAP_THROW_EXCEPTION_MSG( "invalid key type variant for table" );
                        }
                    };
                    auto const resolved_key = resolve_key( key ); // TODO const ref?

                    if constexpr( std::is_same_v< Table, NodeTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, HeadingTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, TitleTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, BodyTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, ChildTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, AliasTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, AttributeTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else if constexpr( std::is_same_v< Table, ResourceTable > )
                    {
                        match_delta_key< Table >( cache, resolved_key, rv );
                    }
                    else
                    {
                        static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
                    }
                }
                , table );

    return rv;
}
#endif // 0

} // namespace kmap::com::db
