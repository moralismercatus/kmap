/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_HPP
#define KMAP_DB_HPP

#include "com/database/cache.hpp"
#include "com/database/common.hpp"
#include "com/database/query_cache.hpp"
#include "common.hpp"
#include "component.hpp"
#include "path.hpp"
#include "utility.hpp" // TODO: Remove. Only reason this is here is for testing.

// #include <sqlpp11/sqlite3/connection.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sqlpp::sqlite3
{
    class connection;
}

namespace kmap::com {

class Database : public Component
{
    FsPath path_ = {};
    std::unique_ptr< sqlpp::sqlite3::connection > con_ = {}; // TODO: I think this actually belongs in com::DatabaseFilesystem. In the future.
    mutable db::Cache cache_ = {}; // Needs to be mutable, as fetching/reading operations are const, but may update the cache. TODO: Really? I think what I had in mind was when it needed to be loaded from disk, but this all happens at one time via explicit command, so I don't think mutable is necessary.
    mutable db::QueryCache query_cache_ = {};

public:
    // using TableId = db::TableId;
    // using ItemValue = db::ValueVariant;
    // using UniqueIdMultiStrSet = DbCache::UniqueIdMultiStrSet;
    // using NonUniqueIdNonUniqueIdSet = DbCache::NonUniqueIdNonUniqueIdSet;
    #if 0
    struct ActionSequence // TODO: Rename to Transaction
    {
        using TableId = db::TableId;
        using ItemValue = db::ValueVariant;
        struct Item
        {
            db::TableId tbl;
            db::KeyVariant const key;
            ItemValue value;
        };
        
        ActionSequence( Database& db );
        ~ActionSequence();

        auto fetch( TableId const& tbl
                  , Uuid const& key )
            -> Result< ItemValue >;
        auto push( TableId const& tbl
                 , db::KeyVariant const& key
                 , ItemValue const& value )
            -> void;
        auto push( Item const& item )
            -> void;
        auto pop()
            -> void;
        auto top()
            -> Item const&;

    private:
        Database& db_;
    };
    #endif // 0

    static constexpr auto id = "database";
    constexpr auto name() const -> std::string_view override { return id; }

    using Component::Component; // TODO: Note that Component( kmap, ... ), but Database doesn't use kmap, so it's actually not necessary. Is there a way to make a component not dependant on Kmap?
    virtual ~Database() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    // Core interface.
    // TODO: Only executing through this 'execute' iface should be enforced, as long as cache relies on these.
    // TODO: Remove this entirely. It bypasses cache which violates constraints.
    template < typename... Args >
    auto execute( Args&&... args )
    {
        fmt::print( "non-const db.execute\n" );
        BC_ASSERT( con_ );

        return ( *con_ )( std::forward< Args >( args )... );
    }
    template < typename... Args >
    auto execute( Args&&... args ) const
    {
        BC_ASSERT( con_ );

        return ( *con_ )( std::forward< Args >( args )... );
    }
    // TODO: There may be a way to make a const-version of this by opening the DB as READONLY.
    auto execute_raw( std::string const& stmt )
        -> std::multimap< std::string, std::string >; // Note: sqlite3 has no distinction of "const", so it can't be enforced even if I wanted to make a const version.
    auto load( FsPath const& path )
        -> Result< void >;
    auto path() const
        -> FsPath;

    // Convenience interface.
    template< typename Table >
    auto contains( auto const& key ) const
        -> bool
    {
        return cache().contains< Table >( key );
    }
    template< typename Table >
    auto erase( auto const& key )
        -> Result< void > 
    {
        return cache().erase< Table >( key );
    }
    template< typename Table >
    auto fetch() const
        -> Table const&
    {
        return cache().fetch< Table >();
    }

    auto has_file_on_disk()
        -> bool;
    auto push_node( Uuid const& id )
        -> Result< void >;
    auto push_body( Uuid const& node
                  , std::string const& body )
        -> Result< void >;
    auto push_heading( Uuid const& node
                     , std::string const& heading )
        -> Result< void >;
    auto push_child( Uuid const& parent 
                   , Uuid const& child )
        -> Result< void >;
    auto push_title( Uuid const& node
                   , std::string const& title )
        -> Result< void >;
    auto push_alias( Uuid const& src 
                   , Uuid const& dst )
        -> Result< void >;
    auto push_attr( Uuid const& parent
                  , Uuid const& attr )
        -> Result< void >;
    // auto fetch_aliases() const
    //     -> NonUniqueIdNonUniqueIdSet;
    auto fetch_alias_destinations( Uuid const& src ) const
        -> Result< std::vector< Uuid > >;
    auto fetch_alias_sources( Uuid const& dst ) const
        -> std::vector< Uuid >;
    auto init_db_on_disk( FsPath const& path )
        -> Result< void >;
    [[ nodiscard ]]
    auto cache() const
        -> db::Cache const&;
    auto create_tables()
        -> void;
    // auto create_node( Uuid const& id
    //                 , Uuid const& parent
    //                 , Heading const& heading
    //                 , Title const& title )
    //     -> bool;
    auto fetch_parent( Uuid const& id ) const
        -> Result< Uuid >;
    auto fetch_child( Uuid const& parent 
                    , Heading const& heading) const
        -> Result< Uuid >;
    auto fetch_body( Uuid const& id ) const
        -> Result< std::string >;
    auto fetch_children() const
        -> std::vector< std::pair< Uuid
                                 , Uuid > >;
    auto fetch_children( Uuid const& id ) const
        -> Result< UuidSet >;
    auto fetch_heading( Uuid const& id ) const
        -> Result< Heading >;
    auto fetch_attr( Uuid const& parent ) const
        -> Result< Uuid >;
    auto fetch_attr_owner( Uuid const& attrn ) const
        -> Result< Uuid >;
    // auto fetch_bodies() const
    //     -> UniqueIdMultiStrSet;
    auto fetch_nodes() const
        -> UuidUnSet;
    auto fetch_nodes( Heading const& heading ) const
        -> UuidSet;
    auto fetch_title( Uuid const& id ) const
        -> Result< std::string >;
    auto fetch_genesis_time( Uuid const& id ) const
        -> Optional< uint64_t >;
    [[ nodiscard ]]
    auto query_cache()
        -> db::QueryCache&;
    [[ nodiscard ]]
    auto query_cache() const
        -> db::QueryCache const&;
    auto update_heading( Uuid const& node
                       , Heading const& heading )
        -> Result< void >;
    auto update_title( Uuid const& node
                     , Title const& title )
        -> Result< void >;
    auto update_body( Uuid const& node
                    , std::string const& content )
        -> Result< void >;
    // TODO: I think all these "exists" "is" utilities can be replaced with template< Table, Key > contains( auto const& key ):
    auto node_exists( Uuid const& id ) const
        -> bool;
    auto attr_exists( Uuid const& id ) const
        -> bool;
    auto alias_exists( Uuid const& src
                     , Uuid const& dst ) const
        -> bool;
    auto erase_alias( Uuid const& src
                    , Uuid const& dst )
        -> Result< void >;
    auto erase_all( Uuid const& id ) // TODO: Ensures all lhs/rhs for all tables is erased ("cascading erase")? Or not DB's responsibilty?
        -> Result< void >;
    auto erase_child( Uuid const& parent
                    , Uuid const& child )
        -> Result< void >;
    auto is_child( Uuid const& parent
                 , Uuid const& id ) const
        -> bool;
    [[ deprecated( "non-core functionality" ) ]]
    auto is_child( Uuid const& parent
                 , Heading const& heading ) const
        -> bool;
    auto has_parent( Uuid const& child ) const
        -> bool;
    auto child_headings( Uuid const& id ) const
        -> std::vector< Heading >;
    auto flush_delta_to_disk()
        -> Result< void >;
    auto has_delta() const
        -> bool;

protected:
    auto cache()
        -> db::Cache&;
    auto load_internal( FsPath const& path )
        -> Result< void >;
//protected: // Allowing access for "repair-state". TODO: Better handle this.
    // auto action_seq()
    //     -> ActionSequence;

    // auto fetch( TableId const& tbl
    //           , Uuid const& key )
    //     -> Result< db::ValueVariant >;
    // auto push( TableId const& tbl
    //          , db::KeyVariant const& id
    //          , ItemValue const& value )
    //     -> Result< void >;
};

auto create_attr_node( Database& db
                     , Uuid const& parent)
    -> Result< Uuid >;
auto create_tables( sqlpp::sqlite3::connection& con )
    -> void;
auto execute_raw( sqlpp::sqlite3::connection& con
                , std::string const& stmt )
    -> std::multimap< std::string, std::string >;

} // namespace kmap::com

#endif // KMAP_DB_HPP
