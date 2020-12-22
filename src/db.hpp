/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_HPP
#define KMAP_DB_HPP

#include "common.hpp"
#include "db_cache.hpp"
#include "db_cache.hpp"
#include "path.hpp"
#include "utility.hpp" // TODO: Remove. Only reason this is here is for testing.

#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/ppgen.h>
#include <sqlpp11/sqlite3/connection.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

SQLPP_DECLARE_TABLE
(
    ( nodes )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( children )
    ,
    ( parent_uuid, text, SQLPP_NOT_NULL  )
    ( child_uuid, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( headings )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( heading, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( titles )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( title, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( bodies )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( body, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( genesis_times )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( unix_time, bigint, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( aliases )
    ,
    ( src_uuid, text, SQLPP_NOT_NULL  )
    ( dst_uuid, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( child_orderings )
    ,
    ( parent_uuid, text, SQLPP_NOT_NULL  )
    ( sequence, text, SQLPP_NOT_NULL  ) // First 8-bytes of child id per child.
)

SQLPP_DECLARE_TABLE
(
    ( resources )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( resource, blob, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( original_resource_sizes )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( resource_size, int, SQLPP_NOT_NULL  )
)

namespace kmap
{

namespace sql = sqlpp::sqlite3;

class Database
{
public:
    using UniqueIdMultiStrSet = DbCache::UniqueIdMultiStrSet;
    using NonUniqueIdNonUniqueIdSet = DbCache::NonUniqueIdNonUniqueIdSet;

    Database( FsPath const& db_path );

    // Core interface.
    // TODO: Only executing through this 'execute' iface should be enforced, as long as cache relies on these.
    template < typename... Args >
    auto execute( Args&&... args )
    {
        #if KMAP_LOGGING_DB_CACHE
        fmt::print( "[DEBUG] db non-const\n" );
        #endif // KMAP_LOGGING_DB_CACHE 

        flush_cache();

        return ( *con_ )( std::forward< Args >( args )... );
    }
    template < typename... Args >
    auto execute( Args&&... args ) const
    {
        #if KMAP_LOGGING_DB_CACHE
        fmt::print( "[DEBUG] db const\n" );
        #endif // KMAP_LOGGING_DB_CACHE 

        return ( *con_ )( std::forward< Args >( args )... );
    }
    // TODO: There may be a way to make a const-version of this by opening the DB as READONLY.
    auto execute_raw( std::string const& stmt )
        -> std::map< std::string, std::string >; // Note: sqlite3 has no distinction of "const", so it can't be enforced even if I wanted to make a const version.
    auto path() const
        -> FsPath;

    // Convenience interface.
    auto add_child( Uuid const& parent 
                  , Uuid const& child )
        -> void;
    auto fetch_aliases() const
        -> NonUniqueIdNonUniqueIdSet;
    auto fetch_alias_destinations( Uuid const& src ) const
        -> std::vector< Uuid >;
    auto fetch_alias_sources( Uuid const& dst ) const
        -> std::vector< Uuid >;
    auto set_defaults()
        -> void;
    auto create_tables()
        -> void;
    auto create_node( Uuid const& id
                    , Uuid const& parent
                    , Heading const& heading
                    , Title const& title )
        -> bool;
    auto create_alias( Uuid const& src 
                     , Uuid const& dst )
        -> bool;
    auto fetch_parent( Uuid const& id ) const
        -> Optional< Uuid >;
    auto fetch_child( Heading const& heading
                    , Uuid const& parent ) const
        -> Optional< Uuid >;
    auto fetch_body( Uuid const& id ) const
        -> Optional< std::string >;
    auto fetch_children() const
        -> std::vector< std::pair< Uuid
                                 , Uuid > >;
    auto fetch_children( Uuid const& id ) const
        -> UuidSet;
    auto fetch_heading( Uuid const& id ) const
        -> Optional< Heading >;
    auto fetch_headings() const
        -> UniqueIdMultiStrSet const&;
    auto fetch_bodies() const
        -> UniqueIdMultiStrSet const&;
    auto fetch_nodes() const
        -> UuidUnSet const&;
    auto fetch_nodes( Heading const& heading ) const
        -> UuidSet;
    auto fetch_title( Uuid const& id ) const
        -> Optional< std::string >;
    auto fetch_genesis_time( Uuid const& id ) const
        -> Optional< uint64_t >;
    auto heading_exists( Heading const& heading ) const
        -> bool;
    auto update_child_ordering( Uuid const& parent
                              , std::vector< std::string > const& abbreviations )
        -> void;
    auto update_heading( Uuid const& id
                       , Heading const& heading )
        -> void;
    auto update_title( Uuid const& id
                     , Title const& title )
        -> void;
    auto update_body( Uuid const& node
                    , std::string const& content )
        -> void;
    auto update_bodies( std::vector< std::pair< Uuid, std::string > > const& updates )
        -> void;
    auto exists( Uuid const& id ) const
        -> bool;
    auto alias_exists( Uuid const& src
                     , Uuid const& dst ) const
        -> bool;
    auto is_child( Uuid const& parent
                 , Uuid const& id ) const
        -> bool;
    auto is_child( Uuid const& parent
                 , Heading const& heading ) const
        -> bool;
    auto has_parent( Uuid const& child ) const
        -> bool;
    auto has_child_ordering( Uuid const& parent ) const
        -> bool;
    auto remove( Uuid const& id ) // TODO: Rename to remove_node to be explicit.
        -> void;
    auto remove_child( Uuid const& parent
                     , Uuid const& child )
        -> void;
    auto remove_alias( Uuid const& src
                     , Uuid const& dst )
        -> void;
    auto child_headings( Uuid const& id ) const
        -> std::vector< Heading >;

//protected: // Allowing access for "repair-state". TODO: Better handle this.
    // The child_orderings table is constrained as part of this class. 
    auto append_child_to_ordering( Uuid const& parent
                                 , Uuid const& child )
        -> void;
    auto fetch_child_ordering( Uuid const& parent ) const
        -> std::vector< std::string >;
    auto remove_alias_from_ordering( Uuid const& src
                                   , Uuid const& dst )
        -> void;
    auto remove_from_ordering( Uuid const& parent
                             , Uuid const& target )
        -> void;
    auto remove_from_ordering( Uuid const& parent
                             , std::string const& order_id )
        -> void;
    auto is_ordered( Uuid const& parent
                   , Uuid const& child ) const
        -> bool;
    auto flush_cache()
        -> void;

private:
    FsPath path_;
    std::unique_ptr< sql::connection > con_;
    mutable DbCache cache_; // Needs to be mutable, as fetching/reading operations are const.
};

} // namespace kmap

#endif // KMAP_DB_HPP
