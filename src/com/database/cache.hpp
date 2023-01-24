/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_CACHE_HPP
#define KMAP_DB_CACHE_HPP

#include "common.hpp"
#include "com/database/sm.hpp"
#include "com/database/common.hpp"
#include "container.hpp"
#include "error/db.hpp"
#include "util/concepts.hpp"
#include "util/result.hpp"

#include <boost/bimap.hpp>
#include <boost/container_hash/hash.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/transform.hpp>

#include <compare>
#include <functional>
#include <optional>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace bmi = boost::multi_index;

namespace kmap::db {

// template< typename Left
//         , typename Right >
// auto hash_value( TableItem< Left, Right > const& item )
//     -> std::size_t
// {
//     auto lhasher = boost::hash< Left >{};
//     auto rhasher = boost::hash< Left >{};
//     auto const lh = lhasher( item.left );
//     auto const rh = rhasher( item.right );

//     boost::hash_combine( lh, rh );
// }

// TODO: Rename to something descriptive? It depends on the use case. Most tables require just the key to be unique. Certain tables (children, alias) require {k,v} to be unique.
// TODO: Even though the key unique-only requires only the key to be unique, it should still make the val indexible, for lookup purposes.
                                                                                    //    , bmi::composite_key< TableItemKV< Left, Right >
                                                                                    //                        , bmi::member< TableItemKV< Left, Right >
                                                                                    //                                     , Left
                                                                                    //                                     , &TableItemKV< Left, Right >::kv::first >
                                                                                    //                        , bmi::member< TableItemKV< Left, Right >
                                                                                    //                                     , Right
                                                                                    //                                     , &TableItemKV< Left, Right >::kv::right > >
                                                                                    //    , bmi::composite_key_hash< boost::hash< Left >
                                                                                    //                             , boost::hash< Right > > >

class CacheDeciderSm;

// TODO: Belongs in own file?
// TODO: Rename to RewindSequence?
// Note: I've determined that there is a distinction between history/undo and a rewind on failure.
//       There _is_ overlap in the notions, that of rewinding or reversing the operations. Further,
//       there is overlap in the collection of a unit of transaction. Either all succeed or all fail, that is.
//       But there is a sharp distinction, as well, that history/undo assumes successful pushes of the entire unit already.
//       Thus, this must be a separate component, else it would constitute a circular dependency. This is what was tripping me up for some time.
//       I should proceed thusly: establish the rewind for failure case, and execute history/undo seperately.
//       There must be some work on establishing the association between the individual failure case and the sequence as a whole.
//       Why not just store the currect squence? Because this sequence mapping needs to apply when the cache is flushed as well. If an item fails to propagate
//       while flushing, the set should be rejected. Err... that last statement is troublesome. If we indeed entertain the possibility of individual failure,
//       what about other data that reference this in the cache? We can't simply rewind, as that would compromise all dependencies.
//       So, the question remains: is a single rewind sequence for the current set sufficient for the time being, and just assuming that flushes can't, or won't, fail?

class UndoStore
{
public:
    struct Item
    { 
        DeltaType action;
        UniqueKeyVariant key;
        ValueVariant value;
    };
    using Items = std::vector< Item >;

    auto delimit()
        -> void;
    auto push( Item const& item )
        -> void;
    auto push( Items const& item )
        -> void;
    auto pop()
        -> void;
    auto top()
        -> Items;

private:
    std::vector< Items > item_groups_ = { Items{} };
};

struct ReversalItem
{
    DeltaType action = {};
    UniqueKeyVariant key = {};
    ValueVariant value = {};
};

class Cache
{
    // Note: There is a dependency among tables. All tables depend, respecting DB semantics, on NodeTable, so it should be listed last such that (erase) iterations find it last.
    using CacheTables = std::tuple< HeadingTable
                                  , TitleTable
                                  , BodyTable
                                  , ChildTable
                                  , AliasTable
                                  , AttributeTable
                                  , ResourceTable
                                  , NodeTable >;
    using ReversalMap = std::map< TransactionIdSet, std::vector< ReversalItem > >;

    CacheTables cache_tables_ = {};
    // UndoStore undo_store_ = {};
    // ReversalMap units_ = {};
    // TransactionIdSet curr_groups_ = {}; // Rather, should it not be a UUID assigned to a group, rather than an index?

    friend class kmap::db::CacheDeciderSm;

public:
    Cache() = default;

    [[ nodiscard ]]
    auto tables() const
        -> CacheTables const&
    {
        return cache_tables_;
    }

    // TODO: Can I return Result< ref< const Table > > instead of throwing an exception?
    template< typename Table >
    [[ nodiscard ]]
    auto fetch() const
        -> Table const&
    {
        return std::get< Table >( cache_tables_ );
        // return std::visit( [ & ]( auto const& arg ) -> Table const&
        //                    {
        //                        using T = std::decay_t< decltype( arg ) >;

        //                        if constexpr( std::is_same_v< T, Table > )
        //                        {
        //                            return arg;
        //                        }
        //                        else
        //                        {
        //                            KMAP_THROW_EXCEPTION_MSG( "unable to find table" );
        //                        }
        //                    }
        //                  , cache_tables_ );
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto fetch_value( Key const& key ) const // TODO: ought this be Table::key_type?
        -> Result< typename Table::value_type >
    {
        auto rv = KMAP_MAKE_RESULT( typename Table::value_type );

        if( auto const dr = fetch_last_delta< Table, Key >( key )
          ; dr )
        {
            if( dr.value().action != DeltaType::erased )
            {
                rv = dr.value().value;
            }
        }
        else if( auto const cr = fetch_cached< Table, Key >( key )
               ; cr )
        {
            rv = cr.value();
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto fetch_values( Key const& key ) const
        -> Result< std::vector< typename Table::template value_type_v< Key > > >
    {
        using value_type = typename Table::template value_type_v< Key >;

        KM_RESULT_PROLOG();

        auto rv = result::make_result< std::vector< value_type > >();
        auto const items = KTRY( fetch_items< Table >( key ) );
        auto spliced = std::set< value_type >{};

        for( auto const& item : items )
        {
            if( !item.delta_items.empty() )
            {
                auto const& di = item.delta_items.back();

                if( di.action != DeltaType::erased )
                {
                    spliced.emplace( std::get< value_type >( di.value ) );
                }
            }
            else
            {
                KMAP_ENSURE_EXCEPT( item.cache_item );

                spliced.emplace( std::get< value_type >( item.cache_item.value() ) );
            }
        }

        if( !spliced.empty() )
        {
            rv = spliced | ranges::to< std::vector >();
        }

        return rv;
    }
    template< typename Table >
    auto apply_delta_to_cache()
        -> void
    {
        auto& table = std::get< Table >( cache_tables_ );
        auto update_set = std::set< typename Table::unique_key_type >{};
        auto erase_set = std::set< typename Table::unique_key_type >{};

        for( auto& item : table )
        {
            if( !item.delta_items.empty() )
            {
                if( item.delta_items.back().action == DeltaType::erased )
                {
                    erase_set.emplace( item.key() );
                }
                else
                {
                    update_set.emplace( item.key() );
                }
            }
        }

        // TODO: BC_ASSERT( set_difference( update_set, erase_set ) == {} );

        {
            auto const update_fn = [ & ]( auto&& table_item )
            {
                table_item.cache_item = table_item.delta_items.back().value;
                table_item.delta_items = {};
            };

            for( auto const& ti_key : update_set )
            {
                KTRYE( table.update( ti_key, update_fn ) );
            }
        }
        {
            for( auto const& ti_key : erase_set )
            {
                KTRYE( table.erase( ti_key ) );
            }
        }
    }
    template< typename Table >
    auto push( typename Table::unique_key_type const& ukey )
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = result::make_result< void >();
        auto decider = make_unique_cache_decider( *this );

        {
            for( auto const ev = sm::ev::Push{ .table = Table{}, .key = ukey, .value = ukey }
               ; !decider.output->done
               ; )
            {
                decider.driver->process_event( ev );
            }
        }

        if( decider.driver->is( boost::sml::state< sm::state::CreateDelta > ) )
        {
            auto&& table = std::get< Table >( cache_tables_ );

            KTRY( table.create( ukey ) );
        }
        else if( decider.driver->is( boost::sml::state< sm::state::UpdateDelta > ) )
        {
            // Q: Is it logical that a key-only entry cannot be updated? The key would change and therefore the entry,
            //    but what if movement happens? I don't think that works. It's a UUID, meaning one should never have to move from one unique to another unique,
            //    although I'm sure one could contrive some scenario in which it's useful, but I think not common enough to justify the infraction.
            //    For example, a certain UUID is required for some property, and we want to convert a node to that UUID, but changing the UUID of the existing
            //    isn't the right thing to do. The right thing to do is update or copy to the proper UUID. Changing UUIDs complicates things too much. Makes reasoning difficult.

            KMAP_THROW_EXCEPTION_MSG( "should never reach; doesn't make sense to update delta on a key-only table" );
        }
        else if( decider.driver->is( boost::sml::state< sm::state::Nop > ) )
        {
            // Nothing to do.
        }
        else
        {
            // TODO: Not really an exception, more like an assertion. We should never reach here, so long as SM is defined properly.
            KMAP_THROW_EXCEPTION_MSG( fmt::format( "cache decider ended in invalid state: {}", decider.output->error_msg ) );
        }

        rv = outcome::success();

        return rv;
    }
    // TODO: Concepts need specifying for Bimap/Map table distinction for updates?
    template< typename Table >
    auto push( typename Table::left_type const& left 
             , typename Table::right_type const& right )
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( void );
        auto const key = [ & ]
        {
            if constexpr( concepts::Pair< typename Table::unique_key_type > )
            {
                return UniqueKeyVariant{ typename Table::unique_key_type{ left, right } };
            }
            else
            {
                return UniqueKeyVariant{ left };
            }
        }();
        auto const value = [ & ]
        {
            if constexpr( concepts::Pair< typename Table::unique_key_type > )
            {
                return ValueVariant{ typename Table::value_type{ left, right } };
            }
            else
            {
                return ValueVariant{ right };
            }
        }();
        auto decider = make_unique_cache_decider( *this );

        for( auto const ev = sm::ev::Push{ .table = Table{}, .key = key, .value = value }
           ; !decider.output->done
           ; )
        {
            decider.driver->process_event( ev );
        }

        if( decider.driver->is( boost::sml::state< sm::state::CreateDelta > ) )
        {
            auto&& table = std::get< Table >( cache_tables_ );

            KTRY( table.create( left, right ) );
        }
        else if( decider.driver->is( boost::sml::state< sm::state::UpdateDelta > ) )
        {
            auto&& table = std::get< Table >( cache_tables_ );

            if constexpr( concepts::Pair< typename Table::unique_key_type > )
            {
                auto const ukey = typename Table::unique_key_type{ left, right };
                auto const update_fn = [ & ]( auto&& table_item )
                {
                    table_item.delta_items.emplace_back( typename Table::DeltaItem{ .value = ukey, .action = DeltaType::changed, .transaction_id = {} } );
                };

                KTRY( table.update( ukey, update_fn ) );
            }
            else
            {
                auto const update_fn = [ & ]( auto&& table_item )
                {
                    table_item.delta_items.emplace_back( typename Table::DeltaItem{ .value = right, .action = DeltaType::changed, .transaction_id = {} } );
                };

                KTRY( table.update( left, update_fn ) );
            }
        }
        else if( decider.driver->is( boost::sml::state< sm::state::Nop > ) )
        {
            // Nothing to do.
        }
        else
        {
            // TODO: Not really an exception, more like an assertion. We should never reach here, so long as SM is defined properly.
            KMAP_THROW_EXCEPTION_MSG( fmt::format( "cache decider ended in invalid state: {}", decider.output->error_msg ) );
        }

        rv = outcome::success();

        return rv;
    }
    template< typename Table >
    auto erase( typename Table::unique_key_type const& ukey )
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( void );
        auto decider = make_unique_cache_decider( *this );

        for( auto const ev = sm::ev::Erase{ .table = Table{}, .key = ukey }
           ; !decider.output->done
           ; )
        {
            decider.driver->process_event( ev );
        }

        if( decider.driver->is( boost::sml::state< sm::state::Error > ) )
        {
            rv = KMAP_MAKE_ERROR_MSG( error_code::db::erase_failed, decider.output->error_msg );
        }
        else
        {
            if( decider.driver->is( boost::sml::state< sm::state::EraseDelta > ) )
            {
                auto&& table = std::get< Table >( cache_tables_ );
                auto const update_fn = [ & ]( auto&& table_item )
                {
                    table_item.delta_items.emplace_back( typename Table::DeltaItem{ .value = typename Table::value_type{}, .action = DeltaType::erased, .transaction_id = {} } );
                };

                KTRY( table.update( ukey, update_fn ) );
            }
            else if( decider.driver->is( boost::sml::state< sm::state::ClearDelta > ) )
            {
                auto&& table = std::get< Table >( cache_tables_ );

                KTRY( table.erase( ukey ) );
            }
            else
            {
                // TODO: Not really an exception, more like an assertion. We should never reach here, so long as SM is defined properly.
                KMAP_THROW_EXCEPTION_MSG( fmt::format( "cache decider ended in invalid state", decider.output->error_msg ) );
            }

            rv = outcome::success();
        }

        return rv;
    }
    template< typename Table >
    auto erase( typename Table::left_type const& left 
              , typename Table::right_type const& right )
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( void );
        auto const key = [ & ]
        {
            if constexpr( concepts::Pair< typename Table::unique_key_type > )
            {
                return UniqueKeyVariant{ typename Table::unique_key_type{ left, right } };
            }
            else
            {
                return UniqueKeyVariant{ left };
            }
        }();
        auto decider = make_unique_cache_decider( *this );

        for( auto const ev = sm::ev::Erase{ .table = Table{}, .key = key }
           ; !decider.output->done
           ; )
        {
            decider.driver->process_event( ev );
        }
        for( auto const ev = sm::ev::Erase{ .table = Table{}, .key = key }
           ; !decider.output->done
           ; )
        {
            decider.driver->process_event( ev );
        }

        if( decider.driver->is( boost::sml::state< sm::state::EraseDelta > ) )
        {
            auto&& table = std::get< Table >( cache_tables_ );
            auto const ukey = typename Table::unique_key_type{ left, right };

            auto const update_fn = [ & ]( auto&& table_item )
            {
                table_item.delta_items.emplace_back( typename Table::DeltaItem{ .value = ukey, .action = DeltaType::erased, .transaction_id = {} } );
            };

            KMAP_TRY( table.update( ukey, update_fn ) );
        }
        else if( decider.driver->is( boost::sml::state< sm::state::ClearDelta > ) )
        {
            auto&& table = std::get< Table >( cache_tables_ );

            KMAP_TRY( table.erase( typename Table::unique_key_type{ left, right } ) );
        }
        else
        {
            // TODO: Not really an exception, more like an assertion. We should never reach here, so long as SM is defined properly.
            KMAP_THROW_EXCEPTION_MSG( fmt::format( "cache decider ended in invalid state", decider.output->error_msg ) );
        }

        rv = outcome::success();

        return rv;
    }
    
    auto cache() const
        -> CacheTables const&;
    // Superceded by returning cache() and finding the deltas. Not the clearest, but most efficient, rather than generating a delta view into the cache.
    // auto delta() const
    //     -> DeltaTables const&;
    
    auto clear_delta()
        -> void;

    template< typename Table
            , typename Key >
    auto contains( Key const& key ) const
        -> bool
    {
        return contains_delta< Table >( key ) || contains_cached< Table >( key );
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto contains_cached( Key const& key ) const
        -> bool
    {
        auto rv = false;
        auto const& table = std::get< Table >( cache_tables_ );

        if( auto const res = table.fetch( key )
          ; res )
        {
            rv = res.value().cache_item.has_value();
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto contains_cached( Key const& key ) const
        -> bool
    {
        auto rv = false;
        auto const& table = std::get< Table >( cache_tables_ );

        if( auto const res = table.fetch( key )
          ; res )
        {
            auto const& resv = res.value();
            
            rv = ( ranges::count_if( resv, []( auto const& ti ){ return ti.cache_item.has_value(); } ) != 0 );
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto contains_delta( Key const& key ) const
        -> bool
    {
        auto rv = false;
        auto const& table = std::get< Table >( cache_tables_ );

        if( auto const res = table.fetch( key )
          ; res )
        {
            rv = !( res.value().delta_items.empty() );
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto contains_delta( Key const& key ) const
        -> bool
    {
        auto rv = false;
        auto const& table = std::get< Table >( cache_tables_ );

        if( auto const res = table.fetch( key )
          ; res )
        {
            auto const& resv = res.value();
            
            rv = ( ranges::count_if( resv, []( auto const& ti ){ return !ti.delta_items.empty(); } ) != 0 );
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto contains_erased_delta( Key const& key ) const
        -> bool
    {
        auto rv = false;
        auto const& table = std::get< Table >( cache_tables_ );

        if( auto const entry = table.fetch( key )
          ; entry )
        {
            auto const& ev = entry.value();

            if( !ev.delta_items.empty() )
            {
                rv = ( ev.delta_items.back().action == DeltaType::erased );
            }
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto contains_erased_delta( Key const& key ) const
        -> bool
    {
        auto rv = false;
        auto const& table = std::get< Table >( cache_tables_ );

        if( auto const res = table.fetch( key )
          ; res )
        {
            auto const& resv = res.value();
            auto const contains_erased = []( auto const& ti )
            {
                return !ti.delta_items.empty() && ti.delta_items.back().action == DeltaType::erased; 
            };
            
            rv = ( ranges::count_if( resv, contains_erased ) != 0 );
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto fetch_cached( Key const& key ) const
        -> Result< typename Table::value_type >
    {
        auto rv = KMAP_MAKE_RESULT( typename Table::value_type );

        if( auto const ti = fetch_item< Table >( key )
          ; ti )
        {
            if( auto const& ci = ti.value().cache_item
              ; ci )
            {
                rv = ci.value();
            }
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto fetch_cached_value( Key const& key ) const
        -> Result< typename Table::value_type >
    {
        auto rv = KMAP_MAKE_RESULT( typename Table::value_type );

        if( auto const ti = fetch_item< Table >( key )
          ; ti )
        {
            if( auto const& ci = ti.value().cache_item
              ; ci )
            {
                rv = ci.value();
            }
        }

        return rv;
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto fetch_cached_values( Key const& key ) const
        -> Result< std::vector< typename Table::template value_type_v< Key > > >
    {
        using namespace ranges;
        using value_type = typename Table::template value_type_v< Key >;

        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( std::vector< value_type > );
        auto const tis = KMAP_TRY( fetch_items< Table >( key ) );
        auto const cis = tis
                       | views::filter( []( auto const& ti ){ return ti.cache_item.has_value(); } )
                       | views::transform( []( auto const& ti ){ return get< value_type >( ti.cache_item.value() ); } )
                       | to< std::vector< value_type > >();

        if( !cis.empty() )
        {
            rv = cis;
        }

        return rv;
    }

    template< typename Table
            , typename Key >
        requires
            requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto fetch_deltas( Key const& key ) const
        -> Result< typename Table::DeltaItems >
    {
        auto rv = KMAP_MAKE_RESULT( typename Table::DeltaItems );

        if( auto const r = fetch_item< Table >( key )
          ; r )
        {
            if( auto const& dis = r.value().delta_items
              ; !dis.empty() )
            {
                rv = dis;
            }
        }

        return rv;
    }

    template< typename Table
            , typename Key >
        requires
            requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto fetch_deltas( Key const& key ) const
        -> Result< std::vector< std::vector< DeltaItem< typename Table::template value_type_v< Key > > > > >
    {
        using namespace ranges;
        using value_type = typename Table::template value_type_v< Key >;

        auto rv = KMAP_MAKE_RESULT( std::vector< std::vector< DeltaItem< value_type > > > );

        if( auto const r = fetch_items< Table >( key )
          ; r )
        {
            // Q: What if ti.delta_items is empty()? We're going to end up with empty vectors. Is that ok, or should they be filtered?
            // TODO: To filter or not on empty deltas?
            auto const& is = r.value();
            auto const dvs = is
                           | views::remove_if( []( auto const& ti ){ return ti.delta_items.empty(); } )
                           | views::transform( []( auto const& ti ){ return ti.delta_items
                                                                          | views::transform( [](auto const& di ){ return DeltaItem< value_type >{ .value = std::get< value_type >( di.value ), .action = di.action, .transaction_id = di.transaction_id }; } )
                                                                          | to< std::vector< DeltaItem< value_type > > >(); } )
                           | to< std::vector< std::vector< DeltaItem< value_type > > > >();

            if( !dvs.empty() )
            {
                rv = dvs;
            }
        }

        return rv;
    }
    // template< typename Table
    //         , typename Key >
    // auto fetch_deltas( Key const& key ) const
    // {
    //     return fetch_deltas< Table, typename Table::combined_type, Key >( key );
    // }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto fetch_last_delta( Key const& key ) const
        -> Result< DeltaItem< typename Table::value_type > >
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( DeltaItem< typename Table::value_type > );
        auto const r = KTRY( fetch_deltas< Table >( key ) ); BC_ASSERT( !r.empty() );

        rv = r.back();

        return rv;
    }
    template< typename Table
            , typename Key >
        requires
            requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto fetch_last_deltas( Key const& key ) const
        -> Result< std::vector< DeltaItem< typename Table::template value_type_v< Key > > > >
    {
        using value_type = typename Table::template value_type_v< Key >;

        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( std::vector< DeltaItem< value_type > > );
        auto const r = KTRY( fetch_deltas< Table >( key ) );

        rv = r
           | ranges::views::transform( []( auto const& e ){ BC_ASSERT( !e.empty() ); return e.back(); } )
           | ranges::to< std::vector< DeltaItem< value_type > > >();

        return rv;
    }

    // auto undo_store() const
    //     -> UndoStore const;
    // auto undo_store()
    //     -> UndoStore;

protected:
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::NonRangeResult; }
    auto fetch_item( Key const& key ) const
    {
        auto const& table = std::get< Table >( cache_tables_ );

        return table.fetch( key );
    }
    template< typename Table
            , typename Key >
        requires requires( Table t ) { { t.fetch( Key{} ) } -> concepts::RangeResult; }
    auto fetch_items( Key const& key ) const
    {
        auto const& table = std::get< Table >( cache_tables_ );

        return table.fetch( key );
    }
    template< typename Table
            , typename Key >
    auto push_cached( Key const& key ) // SetTable variety
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        std::visit( [ & ]( auto const& tbl ) mutable
                    {
                        using T = std::decay_t< decltype( tbl ) >;

                        if constexpr( std::is_same_v< T, Table > )
                        {
                            if( auto const r = tbl.fetch( key )
                              ; r )
                            {
                            }
                            rv = tbl.push( key );
                        }
                    }
                  , cache_tables_ );

        return rv;
    }
    template< typename Table
            , typename Key
            , typename Value >
    auto push_cached( Key const& key
                    , Value const& val ) // MapTable variety
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        std::visit( [ & ]( auto const& tbl ) mutable
                    {
                        using T = std::decay_t< decltype( tbl ) >;

                        if constexpr( std::is_same_v< T, Table > )
                        {
                            rv = tbl.push( key, val );
                        }
                    }
                  , cache_tables_ );

        return rv;
    }
    template< typename Table
            , typename Key >
    auto push_delta( Key const& key
                   , DeltaType const& dt
                   , TransactionIdSet const& groups )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        std::visit( [ & ]( auto const& tbl ) mutable
                    {
                        using T = std::decay_t< decltype( tbl ) >;

                        if constexpr( std::is_same_v< T, Table > )
                        {
                            rv = tbl.push( key );
                        }
                    }
                  , cache_tables_ );

        return rv;
    }
    template< typename Table
            , typename Key
            , typename Value >
    auto push_delta( Key const& key
                   , Value const& val
                   , DeltaType const& dt
                   , TransactionIdSet const& groups )
        -> Result< void >
    {
        auto rv = KMAP_MAKE_RESULT( void );

        std::visit( [ & ]( auto const& tbl ) mutable
                    {
                        using T = std::decay_t< decltype( tbl ) >;

                        if constexpr( std::is_same_v< T, Table > )
                        {
                            rv = tbl.push( key, val );
                        }
                    }
                  , cache_tables_ );

        return rv;
    }
};

auto print_deltas( db::Cache const& cache )
    -> void;

} // namespace kmap::db

#endif // KMAP_DB_HPP
