/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#if KMAP_LOGGING_ALL
    #define KMAP_LOGGING_DB
#endif // KMAP_LOGGING_PATH_ALL

#include "db.hpp"

#include "contract.hpp"
#include "com/filesystem/filesystem.hpp" // TODO: only present until sql db moves to DatabaseFilesystem.
#include "emcc_bindings.hpp"
#include "error/db.hpp"
#include "error/filesystem.hpp"
#include "error/network.hpp"
#include "io.hpp"
#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/reverse.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/chunk.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/transform.hpp>
#include <sqlpp11/sqlite3/insert_or.h>
#include <sqlpp11/sqlpp11.h>

#include <functional>
#include <regex>

// TODO: TESTING
#include <fstream>

using namespace emscripten;
using namespace ranges;
namespace fs = boost::filesystem;

namespace kmap::com {

#if KMAP_LOGGING_DB
auto dbErrorLogCallback( void* pArg
                       , int iErrCode
                       , char const* zMsg )
    -> void
{
    fmt::print( stderr
              , "{} {}\n"
              , iErrCode
              , zMsg );
}
#endif // KMAP_LOGGING_DB

/**
 * Note:
 *   - Use of "unix-dotfile" as a locking mechanism is done because some platforms/VFSs aren't supporting the default locking mechanism for SQLITE.
 *     Requires file{}?vfs=unix-dotfile in the path and SQLITE_OPEN_URI in the flags.
 */
auto make_connection_confg( FsPath const& db_path
                          , int flags )
    -> sql::connection_config
{
    auto cfg = sql::connection_config{};

    cfg.path_to_database = fmt::format( "file:{}?vfs=unix-dotfile", db_path.string() ); 
    cfg.flags = flags | SQLITE_OPEN_URI;
#if KMAP_LOGGING_DB 
    cfg.debug = true;

    sqlite3_config( SQLITE_CONFIG_LOG
                  , dbErrorLogCallback
                  , NULL );
#endif // KMAP_LOGGING_DB

    return cfg;
}

#if 0
Database::ActionSequence::ActionSequence( Database& db )
    : db_{ db }
{
}

Database::ActionSequence::~ActionSequence()
{
}

auto Database::ActionSequence::fetch( TableId const& tbl
                                    , Uuid const& key )
    -> Result< ItemValue >
{
    return db_.fetch( tbl, key );
}

auto Database::ActionSequence::push( TableId const& tbl
                                   , db::KeyVariant const& key
                                   , ItemValue const& value )
    -> void
{
    // db_.push( tbl, key, value );
}

auto Database::ActionSequence::push( Item const& item )
    -> void
{
}

auto Database::ActionSequence::pop()
    -> void
{
}

auto Database::ActionSequence::top()
    -> Item const&
{
    return {};
}
#endif // 0

auto Database::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    fmt::print( "Database :: initialize\n" );

    rv = outcome::success();

    return rv;
}

// TODO: Shouldn't the load-from-db-file functionality actually be in DatabaseFilesystem?
auto Database::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( con_ );
                BC_ASSERT( path_ == sqlite3_db_filename( con_->native_handle(), nullptr ) );
            }
        })
    ;

    path_ = kmap_inst().database_path();

    KMAP_ENSURE( !path_.empty(), error_code::filesystem::file_not_found );

    fmt::print( "Database :: load: {}\n", path_.string() );

    KMAP_LOG_LINE();
    // TODO: OK, load is failing here... reason? Have something to do with unmounting in filesystem::dtor?
    con_ = std::make_unique< sql::connection >( make_connection_confg( path_, SQLITE_OPEN_READONLY ) );

    KMAP_LOG_LINE();
    auto const handle = con_->native_handle();

    KMAP_LOG_LINE();
    // Sqlite3 disables extended error reporting by default. Enable it.
    sqlite3_extended_result_codes( handle, 1 );

    {
        auto proc_table = [ & ]( auto&& table ) mutable
        {
            using namespace db;
            using Table = std::decay_t< decltype( table ) >;

            if constexpr( std::is_same_v< Table, NodeTable > )
            {
                auto t = nodes::nodes{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_node( KTRYE( uuid_from_string(  e.uuid ) ) ) );
                }
            }
            else if constexpr( std::is_same_v< Table, HeadingTable > )
            {
                auto t = headings::headings{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_heading( KTRYE( uuid_from_string( e.uuid ) ), e.heading ) );
                }
            }
            else if constexpr( std::is_same_v< Table, TitleTable > )
            {
                auto t = titles::titles{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_title( KTRYE( uuid_from_string( e.uuid ) ), e.title ) );
                }
            }
            else if constexpr( std::is_same_v< Table, BodyTable > )
            {
                auto t = bodies::bodies{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_body( KTRYE( uuid_from_string( e.uuid ) ), e.body ) );
                }
            }
            else if constexpr( std::is_same_v< Table, ChildTable > )
            {
                auto t = children::children{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_child( KTRYE( uuid_from_string( e.parent_uuid ) ), KTRYE( uuid_from_string( e.child_uuid ) ) ) );
                }
            }
            else if constexpr( std::is_same_v< Table, AliasTable > )
            {
                auto t = aliases::aliases{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_alias( KTRYE( uuid_from_string( e.src_uuid ) ), KTRYE( uuid_from_string( e.dst_uuid ) ) ) );
                }
            }
            else if constexpr( std::is_same_v< Table, AttributeTable > )
            {
                auto t = attributes::attributes{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_attr( KTRYE( uuid_from_string( e.parent_uuid ) ), KTRYE( uuid_from_string( e.child_uuid ) ) ) );
                }
            }
            else if constexpr( std::is_same_v< Table, ResourceTable > )
            {
                // assert( false && "TODO" );
                // KMAP_THROW_EXCEPTION_MSG( "TODO" );
            }
            else
            {
                static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
            }
        };
        auto apply_delta = [ & ]( auto&& table ) mutable
        {
            using Table = std::decay_t< decltype( table ) >;

            cache_.apply_delta_to_cache< Table >();
        };

        boost::hana::for_each( boost::hana::reverse( cache_.tables() ), proc_table );
        boost::hana::for_each( boost::hana::reverse( cache_.tables() ), apply_delta );
    }
    KMAP_LOG_LINE();

    {
        con_ = std::make_unique< sql::connection >( make_connection_confg( path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE ) );

        auto const handle = con_->native_handle();

        // Sqlite3 disables extended error reporting by default. Enable it.
        sqlite3_extended_result_codes( handle, 1 );
    }
    KMAP_LOG_LINE();

    rv = outcome::success();

    return rv;
}

auto Database::init_db_on_disk( FsPath const& path )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( con_ );
                BC_ASSERT( path_ == sqlite3_db_filename( con_->native_handle(), nullptr ) );
            }
        })
    ;

    path_ = path;

    if( path.extension().empty() )
    {
        path_.replace_extension( "kmap" );
    }

    con_ = std::make_unique< sql::connection >( make_connection_confg( path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE ) );

    auto const handle = con_->native_handle();

    // Sqlite3 disables extended error reporting by default. Enable it.
    sqlite3_extended_result_codes( handle, 1 );

    // TODO: Is this necessary anymore, now that cascading is done outside of sqlite?
    // Sqlite3 requires that foriegn key support is enabled for each new connection.
    con_->execute
    (
        R"(
        PRAGMA foreign_keys = ON;
        )"
    );
    // Disabling synchronous improves performance, but can result in a corrupted DB if crash happens mid-write.
    // Currently, the safer synchronous is ENABLED.
    // con_->execute
    // (
    //     R"(
    //     PRAGMA synchronous = OFF;
    //     )"
    // );

    create_tables();

    rv = outcome::success();

    return rv;
}

auto Database::cache() const
    -> db::Cache const&
{
    return cache_;
}

auto Database::create_tables()
    -> void
{
    // Note: "IF NOT EXISTS" is used to allow create_tables to be called for
    // each connection, in case the database does not contain a new table that
    // has been added after original database creation.
    
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS nodes
(   uuid
,   PRIMARY KEY( uuid )
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS children
(   parent_uuid TEXT
,   child_uuid TEXT
,   PRIMARY KEY( parent_uuid
               , child_uuid )
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS headings
(   uuid TEXT
,   heading TEXT
,   PRIMARY KEY( uuid )
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS titles
(   uuid TEXT
,   title TEXT
,   PRIMARY KEY( uuid )
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS bodies
(   uuid TEXT
,   body TEXT
,   PRIMARY KEY( uuid )
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS aliases
(   src_uuid TEXT
,   dst_uuid TEXT
,   PRIMARY KEY( src_uuid
               , dst_uuid )
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS attributes
(   parent_uuid TEXT
,   child_uuid TEXT
,   PRIMARY KEY( parent_uuid
               , child_uuid )
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS resources
(   uuid TEXT
,   resource BLOB
,   PRIMARY KEY( uuid )
);
        )"
    );
}

// TODO: What about case where external actor overwrites db file? `con_` is still valid, and file still exists.
//       Would need exclusive ownership of file on disk?
auto Database::has_file_on_disk()
    -> bool
{
    auto const p = path();

    return !p.string().empty()
        && fs::exists( p )
        && static_cast< bool >( con_ )
        && p == sqlite3_db_filename( con_->native_handle(), nullptr );

    // TODO: SCENARIO( "external actor alters file on disk" )
}

auto Database::push_child( Uuid const& parent 
                         , Uuid const& child )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // TODO: BC_ASSERT( is_child( parent, child ) );
        })
    ;

    KMAP_ENSURE( node_exists( parent ), error_code::network::invalid_node );
    KMAP_ENSURE( node_exists( child ), error_code::network::invalid_node ); 

    KMAP_TRY( cache_.push< db::ChildTable >( db::Parent{ parent }, db::Child{ child } ) );
    // cache_.push( TableId::attributes, child, db::AttributeValue{ fmt::format( "order:{}", 1 ) } );

    rv = outcome::success();

    return rv;
}

auto Database::push_node( Uuid const& id )
    -> Result< void >
{
    KMAP_ENSURE( !node_exists( id ), error_code::network::duplicate_node );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( cache_.push< db::NodeTable >( id ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_heading( Uuid const& node
                           , std::string const& heading )
    -> Result< void >
{
    KMAP_ENSURE_EXCEPT( node_exists( node ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( cache_.push< db::HeadingTable >( node, heading ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_body( Uuid const& node
                        , std::string const& body )
    -> Result< void >
{
    KMAP_ENSURE_EXCEPT( node_exists( node ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( cache_.push< db::BodyTable >( node, body ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_title( Uuid const& node
                         , std::string const& title )
    -> Result< void >
{
    KMAP_ENSURE_EXCEPT( node_exists( node ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( cache_.push< db::TitleTable >( node, title ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_attr( Uuid const& parent
                        , Uuid const& attr )
    -> Result< void >
{
    KMAP_ENSURE_EXCEPT( node_exists( parent ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( cache_.push< db::AttributeTable >( db::Left{ parent }, db::Right{ attr } ) );

    rv = outcome::success();

    return rv;
}

// auto Database::create_node( Uuid const& id
//                           , Uuid const& parent
//                           , Heading const& heading
//                           , Title const& title )
//     -> bool
// {
    // auto const& schild_id = to_string( id );
    // auto nt = nodes::nodes{};
    // auto ht = headings::headings{};
    // auto tt = titles::titles{};
    // auto bt = bodies::bodies{};
    // auto gtt = genesis_times::genesis_times{};

    // execute( insert_into( nt )
    //         . set( nt.uuid = schild_id ) );
    // execute( insert_into( ht )
    //         . set( ht.uuid = schild_id
    //              , ht.heading = heading ) );
    // execute( insert_into( tt )
    //         . set( tt.uuid = schild_id
    //              , tt.title = title ) );
    // add_child( parent
    //          , id );
    // execute( insert_into( bt )
    //         . set( bt.uuid = schild_id
    //              , bt.body = "" ) );
    // execute( insert_into( gtt )
    //         . set( gtt.uuid = schild_id
    //              , gtt.unix_time = present_time() ) );

    // TODO: TBH, I'm not sure that Database is the right place to enforce the requirement that each node has a heading.
    
    // ...of a statement. That is, we should never allow one of these 3 insertions to complete without all 3 insertions completing.
    // I think to use sqlite3 terminology, we need to support rollback.
    // I think this can safely be done, by assuming we're operating on a single cache statement instance.
    // It is up the user of this iface to delimit cache statements. I need to rework the cache to make this work correctly.
    // I think this is the right code, but I'm undecided as to whether create_node() is appropriate for this db iface.
    // Rather, should it be like cache iface and just provide generic insert()/fetch() routines.
    // cache_.push( db::TableId::nodes
    //            , id
    //            , db::NodeValue{} );
    // cache_.push( db::TableId::headings
    //            , id
    //            , db::HeadingValue{ heading } );
    // cache_.push( db::TableId::titles
    //            , id
    //            , db::TitleValue{ title } );

//     return true;
// }

auto Database::push_alias( Uuid const& src 
                         , Uuid const& dst )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( src ) );
            BC_ASSERT( node_exists( dst ) );
            BC_ASSERT( !alias_exists( src, dst ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( alias_exists( src, dst ) );
            }
        })
    ;

    KMAP_TRY( cache_.push< db::AliasTable >( db::Left{ src }, db::Right{ dst } ) );

    rv = outcome::success();

    return rv;
}

auto Database::path() const
    -> FsPath
{
    return path_;
}

auto Database::fetch_alias_destinations( Uuid const& src ) const
    -> Result< std::vector< Uuid > >
{
    auto rv = KMAP_MAKE_RESULT( std::vector< Uuid > );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( node_exists( src ) ); // TODO: Is this warranted, or should I just return {}? Rather, ensure( exists( src ) )
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                // BC_ASSERT( !rv.value().empty() );
                // BC_ASSERT( cache_.fetch_aliases().completed_set.count( src ) != 0 ); 
            }
        })
    ;

    auto r = cache_.fetch_values< db::AliasTable >( db::Left{ src } );

    if( r )
    {
        auto const& rights = r.value();

        rv = rights
           | views::transform( []( auto const& e ){ return e.value(); } )
           | to< std::vector< Uuid > >();
    }
    // TODO: Does it make sense that when no destinations are found, rather than returning an error, returning a valid empty vector?
    // else if( r.error().ec == error_code::db::no_rhs )
    // {
    //     rv = std::vector< Uuid >{};
    // }
    else
    {
        rv = std::vector< Uuid >{}; // TODO: Propagate error if it's no a no_rhs EC (e.g., key/lhs doesn't exist).
    }

    // auto const& cached = cache_.fetch_aliases();

    // if( cached.completed_set.count( src ) != 0 )
    // {
    //     auto const& srcs = cached.set.get< 1 >();

    //     for( auto [ it, end ] = srcs.equal_range( src )
    //        ; it != end
    //        ; ++it )
    //     {
    //         rv.emplace_back( it->second );
    //     }
    // }
    // else
    // {
    //     auto at = aliases::aliases{};
    //     auto rows = execute( select( all_of( at ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
    //                        . from( at )
    //                        . where( at.src_uuid == to_string( src ) ) );

    //     for( auto const& e : rows )
    //     {
    //         rv.emplace_back( uuid_from_string( e.dst_uuid ).value() );

    //         cache_.insert_alias( src
    //                            , uuid_from_string( e.dst_uuid ).value() );
    //     }
    // }

    return rv;
}

auto Database::fetch_alias_sources( Uuid const& dst ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( dst ) ); // TODO: Is this warranted, or should I just return {}?
        })
    ;

    KMAP_THROW_EXCEPTION_MSG( "Impl. needed" );

    // auto const& cached = cache_.fetch_aliases();

    // if( cached.completed_set.count( dst ) != 0 )
    // {
    //     auto const& srcs = cached.set.get< 2 >();

    //     for( auto [ it, end ] = srcs.equal_range( dst )
    //        ; it != end
    //        ; ++it )
    //     {
    //         rv.emplace_back( it->first );
    //     }
    // }
    // else
    // {
    //     auto at = aliases::aliases{};
    //     auto rows = execute( select( all_of( at ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
    //                             . from( at )
    //                             . where( at.dst_uuid == to_string( dst ) ) );
    //     for( auto const& e : rows )
    //     {
    //         rv.emplace_back( uuid_from_string( e.src_uuid ).value() );

    //         cache_.insert_alias( uuid_from_string( e.src_uuid ).value()
    //                            , dst );
    //     }
    // }

    return rv;
}

auto Database::fetch_parent( Uuid const& id ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( node_exists( id ), error_code::network::invalid_node ); // TODO: replace with ensure_result

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_child( rv.value(), id ) );
            }
        })
    ;

    auto const parent = KMAP_TRY( cache_.fetch_values< db::ChildTable >( db::Child{ id } ) );

    BC_ASSERT( parent.size() == 1 );

    rv = parent.at( 0 ).value();
    
    return rv;
}

// TODO: This probably doesn't belong in Database. It's a utility.
auto Database::fetch_child( Uuid const& parent 
                          , Heading const& heading ) const
    -> Result< Uuid >
{
    KMAP_ENSURE_EXCEPT( node_exists( parent ) ); // TODO: replace with ensure_result

    // TODO: This assumes that every node has a heading. Is this constraint enforced? The only way would be to force Database::create_node( id, heading );
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const children = KMAP_TRY( fetch_children( parent ) );
    auto const headings = children
                        | views::transform( [ & ]( auto const& e ){ return std::make_pair( e, KMAP_TRYE( fetch_heading( e ) ) ); } )
                        | views::filter( [ & ]( auto const& e ){ return e.second == heading; } )
                        | to_vector;
    
    if( !headings.empty() )
    {
        BC_ASSERT( headings.size() == 1 );

        rv = headings.front().first;
    }

    return rv;
}

auto Database::fetch_body( Uuid const& id ) const
    -> Result< std::string >
{
    KMAP_ENSURE_EXCEPT( node_exists( id ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( std::string );

    rv = KMAP_TRY( cache_.fetch_value< db::BodyTable >( id ) );

    return rv;
}

auto Database::fetch_heading( Uuid const& id ) const
    -> Result< Heading >
{
    KMAP_ENSURE_EXCEPT( node_exists( id ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( Heading );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( id ) );
        })
    ;

    rv = KMAP_TRY( cache_.fetch_value< db::HeadingTable >( id ) );

    return rv;
}

auto Database::fetch_attr( Uuid const& id ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( node_exists( id ), error_code::network::invalid_node );

    BC_CONTRACT()
        BC_POST([ & ]
        {
        })
    ;

    // TODO: attr should only ever 1 parent, so the table type should represent this.
    auto const attr = KMAP_TRY( cache_.fetch_values< db::AttributeTable >( db::Left{ id } ) );

    BC_ASSERT( attr.size() == 1 );

    rv = attr.at( 0 ).value();

    return rv;
}

auto Database::fetch_attr_owner( Uuid const& attrn ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    
    KMAP_ENSURE( node_exists( attrn ), error_code::network::invalid_node );

    // TODO: attr should only ever 1 parent, so the table type should represent this.
    auto const attr = KMAP_TRY( cache_.fetch_values< db::AttributeTable >( db::Right{ attrn } ) );

    BC_ASSERT( attr.size() == 1 );

    rv = attr.at( 0 ).value();

    return rv;
}

auto Database::fetch_nodes( Heading const& heading ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const& ht = fetch< db::HeadingTable >().underlying();
    auto const& hv = ht.get< db::right_ordered >();

    for( auto [ it, end ] = hv.equal_range( heading )
       ; it != end
       ; ++it )
    {
        rv.emplace( it->left() );
    }

    return rv;
}

// auto Database::fetch_bodies() const
//     -> UniqueIdMultiStrSet
// {
//     KMAP_THROW_EXCEPTION_MSG( "Impl. needed" );
    // if( !cache_.fetch_bodies().set_complete )
    // {
    //     auto ht = bodies::bodies{};
    //     auto rows = execute( select( all_of( ht ) )
    //                        . from( ht )
    //                        . unconditionally() );
        
    //     for( auto const& e : rows )
    //     {
    //         cache_.insert_body( uuid_from_string( e.uuid ).value()
    //                           , e.body );
    //     }

    //     cache_.mark_body_set_complete();
    // }

    // return cache_.fetch_bodies().set;
//     return {};
// }

auto Database::fetch_title( Uuid const& id ) const
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );

    rv = KMAP_TRY( cache_.fetch_value< db::TitleTable >( id ) );

    // auto const& titles = cache_.fetch_titles();
    // auto const& map = titles.set.get< 1 >();

    // if( auto const p = map.find( id )
    //   ; p != map.end() )
    // {
    //     rv = p->second;
    // }
    // else// if( titles.completed_set.count( id ) == 0 ) // Note: Should never have node without title.
    // {
    //     auto const& sid = to_string( id );
    //     auto tt = titles::titles{};
    //     auto const& rt = execute( select( all_of( tt ) )
    //                             . from( tt )
    //                             . where( tt.uuid == sid ) );

    //     if( !rt.empty() )
    //     {
    //         rv = rt.front().title;

    //         cache_.insert_title( id
    //                            , *rv );
    //     }
    // }

    return rv;
}

auto Database::fetch_genesis_time( Uuid const& id ) const
    -> Optional< uint64_t >
{
    KMAP_THROW_EXCEPTION_MSG( "deprecated" );
    auto rv = Optional< uint64_t >{};
    // auto const& sid = to_string( id );
    // auto gtt = genesis_times::genesis_times{};
    // auto const& rt = execute( select( all_of( gtt ) )
    //                         . from( gtt )
    //                         . where( gtt.uuid == sid ) );

    // if( !rt.empty() )
    // {
    //     rv = rt.front().unix_time;
    // }

    return rv;
}

auto Database::fetch_nodes() const
    -> UuidUnSet
{
    KMAP_THROW_EXCEPTION_MSG( "Impl. needed" );
    // if( auto const nodes = cache_.fetch_nodes()
    //   ; !nodes.set_complete )
    // {
    //     auto nt = nodes::nodes{};
    //     auto rows = execute( select( all_of( nt ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
    //                        . from( nt )
    //                        . unconditionally() );

    //     for( auto const& e : rows )
    //     {
    //         cache_.insert_node( uuid_from_string( e.uuid ).value() );
    //     }

    //     cache_.mark_node_set_complete();
    // }

    // return cache_.fetch_nodes().set;
    return {};
}

auto Database::update_heading( Uuid const& node
                             , Heading const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_heading( node ).value() == heading );
            }
        })
    ;

    KMAP_ENSURE( node_exists( node ), error_code::network::invalid_node );

    KMAP_TRY( cache_.push< db::HeadingTable >( node, heading ) );

    rv = outcome::success();

    return rv;
}

auto Database::update_title( Uuid const& node
                           , Title const& title )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_title( node ).value() == title );
            }
        })
    ;

    KMAP_ENSURE( node_exists( node ), error_code::network::invalid_node );

    KMAP_TRY( cache_.push< db::TitleTable >( node, title ) );

    rv = outcome::success();

    return rv;
}

auto Database::update_body( Uuid const& node
                          , std::string const& content )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( node_exists( node ), error_code::network::invalid_node );

    KMAP_TRY( cache_.push< db::BodyTable >( node, content ) );

    rv = outcome::success();

    return rv;
}

auto Database::node_exists( Uuid const& id ) const
    -> bool
{
    return !cache_.contains_erased_delta< db::NodeTable >( id )
        && ( cache_.contains_cached< db::NodeTable >( id )
          || cache_.contains_delta< db::NodeTable >( id ) );
}

auto Database::attr_exists( Uuid const& id ) const
    -> bool
{
    auto const key = db::Child{ id };

    return !cache_.contains_erased_delta< db::AttributeTable >( key )
        && ( cache_.contains_cached< db::AttributeTable >( key )
          || cache_.contains_delta< db::AttributeTable >( key ) );
}

auto Database::alias_exists( Uuid const& src
                           , Uuid const& dst ) const
    -> bool
{
    auto const key = db::AliasTable::unique_key_type{ db::Src{ src }, db::Dst{ dst } };

    return !cache_.contains_erased_delta< db::AliasTable >( key )
        && ( cache_.contains_cached< db::AliasTable >( key )
          || cache_.contains_delta< db::AliasTable >( key ) );
}

auto Database::is_child( Uuid const& parent
                       , Uuid const& id ) const
    -> bool
{
    auto const& children = KMAP_TRYE( fetch_children( parent ) ); // TODO: Replace with KMAP_TRY().

    return children.find( id ) != children.end();
}

auto Database::is_child( Uuid const& parent
                       , Heading const& heading ) const
    -> bool
{
    return fetch_child( parent, heading ).has_value();
}

auto Database::has_parent( Uuid const& child ) const
    -> bool
{
    return fetch_parent( child ).has_value();
}

auto Database::erase_all( Uuid const& id )
    -> void // TODO: Why not Result< void >?
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
        BC_POST([ & ]
        {
        })
    ;

    // This needs to delete all table items with LHS or RHS (as applicable) is ID; "Cascade."

    auto const fn = [ & ]( auto const& table )
    {
        using Table = std::decay_t< decltype( table ) >;

        if constexpr( std::is_same_v< Table, db::NodeTable > )
        {
            if( contains< Table >( id ) )
            {
                KMAP_TRYE( cache_.erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::HeadingTable > )
        {
            if( contains< Table >( id ) )
            {
                KMAP_TRYE( cache_.erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::TitleTable > )
        {
            if( contains< Table >( id ) )
            {
                KMAP_TRYE( cache_.erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::BodyTable > )
        {
            if( cache_.contains< Table >( id ) )
            {
                KMAP_TRYE( cache_.erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::ResourceTable > )
        {
            if( cache_.contains< Table >( id ) )
            {
                KMAP_TRYE( cache_.erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::AttributeTable > )
        {
            if( auto const parent = fetch_attr_owner( id )
              ; parent )
            {
                KMAP_TRYE( cache_.erase< Table >( db::Parent{ parent.value() }, db::Child{ id } ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::ChildTable > )
        {
            if( auto const parent = fetch_parent( id )
              ; parent )
            {
                KMAP_TRYE( cache_.erase< Table >( db::Parent{ parent.value() }, db::Child{ id } ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::AliasTable > )
        {
            // TODO: Deleting alias dsts is a problem, in current state. Doesn't mesh with current alias gen system.
            //       Actually, it _could_, so long as those are deleted also (preferably before this is called).
            if( auto const dsts = fetch_alias_destinations( id )
              ; dsts )
            {
                auto const& dstsv = dsts.value();

                for( auto const& dst : dstsv )
                {
                    KMAP_TRYE( cache_.erase< Table >( db::Src{ id }, db::Dst{ dst } ) );
                }
            }
        }
        else
        {
            static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
        }
    };

    boost::hana::for_each( cache_.tables(), fn );
}

auto Database::fetch_children() const
    -> std::vector< std::pair< Uuid, Uuid > >
{
    auto rv = std::vector< std::pair< Uuid, Uuid > >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                node_exists( e.first );
                node_exists( e.second );
            }
        })
    ;

    KMAP_THROW_EXCEPTION_MSG( "Impl. needed" );

    // TODO: Return from cache.

    // auto ct = children::children{};
    // auto rows = execute( select( all_of( ct ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
    //                           . from( ct )
    //                           . unconditionally() );
    // rv = [ & ] // Note: for some reason, views::transform doesn't seem compatible with 'rows'.
    // {
    //     auto ps = std::vector< std::pair< Uuid
    //                                     , Uuid > >{};

    //     for( auto const& e : rows )
    //     {
    //         ps.emplace_back( uuid_from_string( e.parent_uuid ).value()
    //                        , uuid_from_string( e.child_uuid ).value() );
    //     }

    //     return ps;
    // }();

    return rv;
}

auto Database::fetch_children( Uuid const& parent ) const
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );

    BC_CONTRACT()
        BC_POST([ & ]
        {
        })
    ;

    KMAP_ENSURE( node_exists( parent ), error_code::network::invalid_node );

    if( auto const vs = cache_.fetch_values< db::ChildTable >( db::Parent{ parent } )
      ; vs )
    {
        rv = vs.value()
           | views::transform( []( auto const& e ){ return e.value(); } )
           | to< UuidSet >();
    }
    else
    {
        rv = UuidSet{};
    }

    return rv;
}

// TODO: rename get/fetch_headings
auto Database::child_headings( Uuid const& id ) const
    -> std::vector< Heading >
{
    KMAP_THROW_EXCEPTION_MSG( "Impl. needed" );

    auto rv = std::vector< Heading >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( id ) );
        })
        BC_POST([ & ]
        {
            // BC_ASSERT( fetch_children( id ).size() == rv.size() );
        })
    ;

    auto const children = fetch_children( id );

    // rv = children
    //    | views::transform( [ & ]( auto const& e ){ return fetch_heading( e ).value(); } )
    //    | to< std::vector< Heading > >();

    return rv;
}

auto Database::erase_child( Uuid const& parent
                          , Uuid const& child )
    -> void
{
    KMAP_THROW_EXCEPTION_MSG( "Impl. needed" );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( parent ) );
            BC_ASSERT( node_exists( child ) );
            BC_ASSERT( is_child( parent
                               , child ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !is_child( parent
                                , child ) );
        })
    ;

    auto ct = children::children{};

    execute( remove_from( ct ) 
           . where( ct.parent_uuid == to_string( parent )
                 && ct.child_uuid == to_string( child ) ) );
}

auto Database::erase_alias( Uuid const& src
                          , Uuid const& dst )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( src ) );
            BC_ASSERT( node_exists( dst ) );
            BC_ASSERT( alias_exists( src, dst ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !alias_exists( src, dst ) );
            }
        })
    ;

    KMAP_TRY( cache_.erase< db::AliasTable >( db::Src{ src }, db::Dst{ dst } ) );
    
    rv = outcome::success();

    return rv;

    // auto at = aliases::aliases{};

    // execute( remove_from( at ) 
    //        . where( at.src_uuid == to_string( src )
    //              && at.dst_uuid == to_string( dst ) ) );
}

auto Database::flush_delta_to_disk()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !has_delta() );
            }
        })
    ;

    KMAP_ENSURE( has_file_on_disk(), error_code::common::uncategorized );

    auto proc_table = [ & ]( auto&& table ) mutable
    {
        using namespace db;
        using sqlpp::sqlite3::insert_or_replace_into;
        using sqlpp::dynamic_remove_from;
        using Table = std::decay_t< decltype( table ) >;

        auto nt = nodes::nodes{};
        auto nt_ins = insert_into( nt ).columns( nt.uuid ); // Nodes should never be "replaced"/updated.
        auto nt_rm = dynamic_remove_from( (*con_), nt ).dynamic_where();
        auto ht = headings::headings{};
        auto ht_ins = insert_or_replace_into( ht ).columns( ht.uuid, ht.heading );
        auto ht_rm = dynamic_remove_from( (*con_), ht ).dynamic_where();
        auto tt = titles::titles{};
        auto tt_ins = insert_or_replace_into( tt ).columns( tt.uuid, tt.title );
        auto tt_rm = dynamic_remove_from( (*con_), tt ).dynamic_where();
        auto bt = bodies::bodies{};
        auto bt_ins = insert_or_replace_into( bt ).columns( bt.uuid, bt.body );
        auto bt_rm = dynamic_remove_from( (*con_), bt ).dynamic_where();
        auto ct = children::children{};
        auto ct_ins = insert_into( ct ).columns( ct.parent_uuid, ct.child_uuid );
        auto ct_rm = dynamic_remove_from( (*con_), ct ).dynamic_where();
        auto at = aliases::aliases{};
        auto at_ins = insert_into( at ).columns( at.src_uuid, at.dst_uuid );
        auto at_rm = dynamic_remove_from( (*con_), at ).dynamic_where();
        auto att = attributes::attributes{};
        auto att_ins = insert_into( att ).columns( att.parent_uuid, att.child_uuid );
        auto att_rm = dynamic_remove_from( (*con_), att ).dynamic_where();

        for( auto const& item : table )
        {
            if constexpr( std::is_same_v< Table, NodeTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        nt_rm.where.add( nt.uuid == to_string( item.key() ) );
                    }
                    else
                    {
                        nt_ins.values.add( nt.uuid = to_string( item.key() ) );
                    }
                }
            }
            else if constexpr( std::is_same_v< Table, HeadingTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        ht_rm.where.add( ht.uuid == to_string( item.key() ) );
                    }
                    else
                    {
                        ht_ins.values.add( ht.uuid = to_string( item.key() ), ht.heading = dis.back().value );
                    }
                }
            }
            else if constexpr( std::is_same_v< Table, TitleTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        tt_rm.where.add( tt.uuid == to_string( item.key() ) );
                    }
                    else
                    {
                        tt_ins.values.add( tt.uuid = to_string( item.key() ), tt.title = dis.back().value );
                    }
                }
            }
            else if constexpr( std::is_same_v< Table, BodyTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        bt_rm.where.add( bt.uuid == to_string( item.key() ) );
                    }
                    else
                    {
                        bt_ins.values.add( bt.uuid = to_string( item.key() ), bt.body = dis.back().value );
                    }
                }
            }
            else if constexpr( std::is_same_v< Table, ChildTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        ct_rm.where.add( ct.parent_uuid == to_string( item.left().value() )
                                      && ct.child_uuid == to_string( item.right().value() ) );
                    }
                    else
                    {
                        ct_ins.values.add( ct.parent_uuid = to_string( dis.back().value.first.value() )
                                         , ct.child_uuid = to_string( dis.back().value.second.value() ) );
                    }
                }
            }
            else if constexpr( std::is_same_v< Table, AliasTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        at_rm.where.add( at.src_uuid == to_string( item.left().value() )
                                      && at.dst_uuid == to_string( item.right().value() ) );
                    }
                    else
                    {
                        at_ins.values.add( at.src_uuid = to_string( dis.back().value.first.value() )
                                         , at.dst_uuid = to_string( dis.back().value.second.value() ) );
                    }
                }
            }
            else if constexpr( std::is_same_v< Table, AttributeTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        att_rm.where.add( att.parent_uuid == to_string( item.left().value() )
                                       && att.child_uuid == to_string( item.right().value() ) );
                    }
                    else
                    {
                        att_ins.values.add( att.parent_uuid = to_string( dis.back().value.first.value() )
                                          , att.child_uuid = to_string( dis.back().value.second.value() ) );
                    }
                }
            }
            else if constexpr( std::is_same_v< Table, ResourceTable > )
            {
                // Frankly... this one is a bit of a toughy because the size of the resource may be very large.
                // It may make more general sense to pass around a file string, res heading, or some such that, at this point,
                // the file may be stored into the db, so we don't have huge binaries sitting around in the cache. It'll take some thought.
                assert( false && "TODO" );
                KMAP_THROW_EXCEPTION_MSG( "TODO" );
            }
            else
            {
                static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
            }
        }

        auto push_to_db = [ & ]( auto const& ins, auto const& rm ) mutable
        {
            if( !ins.values._data._insert_values.empty()
             || !rm.where._data._dynamic_expressions.empty() )
            {
                cache_.apply_delta_to_cache< Table >();
            }
            if( !ins.values._data._insert_values.empty() )
            {
                execute( ins );
            }
            if( !nt_rm.where._data._dynamic_expressions.empty() )
            {
                execute( rm );
            }
        };

        if constexpr( std::is_same_v< Table, NodeTable > ) { push_to_db( nt_ins, nt_rm ); }
        else if constexpr( std::is_same_v< Table, HeadingTable > ) { push_to_db( ht_ins, ht_rm ); }
        else if constexpr( std::is_same_v< Table, TitleTable > ) { push_to_db( tt_ins, tt_rm ); }
        else if constexpr( std::is_same_v< Table, BodyTable > ) { push_to_db( bt_ins, bt_rm ); }
        else if constexpr( std::is_same_v< Table, ChildTable > ) { push_to_db( ct_ins, ct_rm ); }
        else if constexpr( std::is_same_v< Table, AliasTable > ) { push_to_db( at_ins, at_rm ); }
        else if constexpr( std::is_same_v< Table, AttributeTable > ) { push_to_db( att_ins, att_rm ); }
        else if constexpr( std::is_same_v< Table, ResourceTable > ) { /*TODO*/ }
        else { static_assert( always_false< Table >::value, "non-exhaustive visitor!" ); }
    };

    boost::hana::for_each( cache_.tables(), proc_table );

    rv = outcome::success();

    return rv;
}

auto Database::has_delta() const
    -> bool
{
    auto rv = false;

    auto const fn = [ & ]( auto const& table )
    {
        // TODO: Very inefficient, iterating through _all_ items in _all_ tables, looking for a delta.
        //       Makes more sense to keep a delta flag in each table to consult.
        for( auto const& item : table )
        {
            if( !item.delta_items.empty() )
            {
                rv = true;
                // TODO: break? Is there a way with Boost.Hana?
            }
        }
    };

    boost::hana::for_each( cache_.tables(), fn );

    return rv;
}

// auto Database::action_seq()
//     -> ActionSequence
// {
//     return ActionSequence{ *this };
// }

// auto Database::fetch( TableId const& tbl
//                     , Uuid const& key )
//     -> Result< ItemValue >
// {
//     return KMAP_MAKE_RESULT( ItemValue );
//     // return cache_.fetch( tbl, key );
// }

// auto Database::push( TableId const& tbl
//                    , db::Key const& id
//                    , ItemValue const& value )
//     -> Result< void >
// {
//     return cache_.push( tbl, id, value );
// }

// TODO: I don't know if execute_raw should even be public... very dangerous, as the cache isn't reflected.
auto Database::execute_raw( std::string const& stmt )
    -> std::map< std::string, std::string >
{
    {
        KMAP_ENSURE_EXCEPT( has_file_on_disk() );

        KMAP_TRYE( flush_delta_to_disk() );
    }

    auto rv = std::map< std::string, std::string >{};
    auto const handle = con_->native_handle();
    char* err_msg = {};
    auto const callback = []( void* output
                            , int argc
                            , char* argv[]
                            , char* name[] )
    {
        auto& sout = *static_cast< std::map< std::string, std::string >* >( output );

        if( argc == 2 )// Assumes all tables are in the form k,v
        {
            sout.emplace( std::string{ argv[ 0 ] ? argv[ 0 ] : "NULL" }
                        , std::string{ argv[ 1 ] ? argv[ 1 ] : "NULL" } );
        }
        else
        {
            sout.emplace( "error"
                        , fmt::format( "[db][error] Row cardinality invalid. Expected all tables to be in form k,v" ) );

            return 1;
        }

        return 0;
    };

    if( auto const rc = sqlite3_exec( handle
                                    , stmt.c_str()
                                    , callback
                                    , static_cast< void* >( &rv )
                                    , &err_msg )
      ; rc != SQLITE_OK )
    {
        rv.emplace( "error"
                  ,  err_msg );

        sqlite3_free( err_msg );
    }

    return rv;
}

auto create_attr_node( Database& db
                     , Uuid const& parent )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const mn = gen_uuid();

    KTRY( db.push_node( mn ) );
    KTRY( db.push_heading( mn, "$" ) );
    KTRY( db.push_title( mn, "$" ) );
    KTRY( db.push_attr( parent, mn ) );

    rv = mn;

    return rv;
}


namespace binding {

using namespace emscripten;

struct Database
{
    Kmap& kmap_;

    Database( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto init_db_on_disk( std::string const& path )
        -> kmap::binding::Result< void >
    {
        auto const db = KTRY( kmap_.fetch_component< com::Database >() );

        return db->init_db_on_disk( com::kmap_root_dir / path );
    }

    auto flush_delta_to_disk()
        -> kmap::binding::Result< void >
    {
        try
        {
            auto const db = KTRY( kmap_.fetch_component< com::Database >() );

            return db->flush_delta_to_disk();
        }
        catch( std::exception const& e )
        {
            return kmap::Result< void >{ KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, e.what() ) };
        }
    }

    auto has_delta()
        -> bool
    {
        auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

        return db->has_delta();
    }

    auto has_file_on_disk()
        -> bool
    {
        auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

        return db->has_file_on_disk();
    }
};

auto database()
    -> binding::Database
{
    return binding::Database{ Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_database )
{
    function( "database", &kmap::com::binding::database );
    class_< kmap::com::binding::Database >( "Database" )
        .function( "init_db_on_disk", &kmap::com::binding::Database::init_db_on_disk )
        .function( "flush_delta_to_disk", &kmap::com::binding::Database::flush_delta_to_disk )
        .function( "has_delta", &kmap::com::binding::Database::has_delta )
        .function( "has_file_on_disk", &kmap::com::binding::Database::has_file_on_disk )
        ;
}

} // namespace binding

namespace {
namespace database_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Database
,   std::set({ "component_store"s, "filesystem"s }) // TODO: filesystem shouldn't be a dep. Rather, the SQL functionality (including load from file) needs to move to db_fs.
,   "database related functionality"
);

} // namespace database_store_def 
}

} // namespace kmap::com
