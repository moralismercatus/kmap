/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#if KMAP_LOGGING_ALL
    #define KMAP_LOGGING_DB
    #define KMAP_LOGGING_DB_CACHE
#endif // KMAP_LOGGING_PATH_ALL

#include "db.hpp"

#include "contract.hpp"
#include "io.hpp"
#include "utility.hpp"

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

using namespace emscripten;
using namespace ranges;
namespace fs = boost::filesystem;

namespace kmap
{

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

auto make_connection_confg( FsPath const& db_path )
    -> sql::connection_config
{
    auto cfg = sql::connection_config{};

    cfg.path_to_database = db_path.string(); 
    cfg.flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
#if KMAP_LOGGING_DB
    cfg.debug = true;

    sqlite3_config( SQLITE_CONFIG_LOG
                  , dbErrorLogCallback
                  , NULL );
#endif // KMAP_LOGGING_DB

    return cfg;
}

auto sqlite_custom_regex( sqlite3_context* ctx
                        , int argc
                        , sqlite3_value* argv[] )
    -> void
{
    auto rv = 0;

    if( argc == 2 )
    {
        auto const rgx = std::regex{ reinterpret_cast< char const* >( sqlite3_value_text( argv[ 0 ] ) ) };
        auto const target = std::string{ reinterpret_cast< char const* >( sqlite3_value_text( argv[ 1 ] ) ) };
        auto matches = std::smatch{};

        if( std::regex_search( target
                             , matches
                             , rgx )  )
        {
            rv = 1;
        }
    }
    else
    {
        sqlite3_result_error( ctx
                            , "[db][error] sqlite3 regexp called with invalide number of arguments"
                            , -1 );
        return;
    }

    sqlite3_result_int( ctx
                      , rv );
}

Database::Database( FsPath const& db_path )
    : path_{ db_path }
    , con_{ std::make_unique< sql::connection >( make_connection_confg( db_path ) ) }
{
    set_defaults();
}

auto Database::set_defaults()
    -> void
{
    auto const handle = con_->native_handle();

    // Sqlite3 requires that foriegn key support is enabled for each new connection.
    con_->execute
    (
        R"(
        PRAGMA foreign_keys = ON;
        )"
    );

    // Sqlite3 does not provide a default regex handler for the REGEXP operator.
    if( auto const res = sqlite3_create_function( handle
                                                , "regexp"
                                                , 2
                                                , SQLITE_ANY
                                                , 0
                                                , &sqlite_custom_regex
                                                , 0
                                                , 0 )
      ; res != SQLITE_OK )
    {
        fmt::print( "[db][error] failed to create custom regex handler\n" );
    }
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
,   FOREIGN KEY( parent_uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
,   FOREIGN KEY( child_uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
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
,   FOREIGN KEY( uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
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
,   FOREIGN KEY( uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
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
,   FOREIGN KEY( uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS genesis_times
(   uuid TEXT
,   unix_time INTEGER
,   PRIMARY KEY( uuid )
,   FOREIGN KEY( uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
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
,   FOREIGN KEY( src_uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
,   FOREIGN KEY( dst_uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS child_orderings
(   parent_uuid TEXT
,   sequence TEXT
,   PRIMARY KEY( parent_uuid )
,   FOREIGN KEY( parent_uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
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
,   FOREIGN KEY( uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
);
        )"
    );
    con_->execute
    (
        R"(
CREATE TABLE IF NOT EXISTS original_resource_sizes
(   uuid TEXT
,   resource_size INT
,   PRIMARY KEY( uuid )
,   FOREIGN KEY( uuid )
        REFERENCES nodes( uuid )
        ON DELETE CASCADE
);
        )"
    );
}

auto Database::add_child( Uuid const& parent 
                        , Uuid const& child )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
            BC_ASSERT( exists( child ) );
            BC_ASSERT( !is_ordered( parent
                                  , child ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( is_child( parent 
                               , child ) );
            BC_ASSERT( is_ordered( parent
                                 , child ) );
        })
    ;

    {
        auto ct = children::children{};

        execute( insert_into( ct )
            . set( ct.parent_uuid = to_string( parent )
                 , ct.child_uuid = to_string( child ) ) );
    }

    append_child_to_ordering( parent
                            , child );
}

auto Database::create_node( Uuid const& id
                          , Uuid const& parent
                          , Heading const& heading
                          , Title const& title )
    -> bool
{
    auto const& schild_id = to_string( id );
    auto nt = nodes::nodes{};
    auto ht = headings::headings{};
    auto tt = titles::titles{};
    auto bt = bodies::bodies{};
    auto gtt = genesis_times::genesis_times{};

    execute( insert_into( nt )
            . set( nt.uuid = schild_id ) );
    execute( insert_into( ht )
            . set( ht.uuid = schild_id
                 , ht.heading = heading ) );
    execute( insert_into( tt )
            . set( tt.uuid = schild_id
                 , tt.title = title ) );
    add_child( parent
             , id );
    execute( insert_into( bt )
            . set( bt.uuid = schild_id
                 , bt.body = "" ) );
    execute( insert_into( gtt )
            . set( gtt.uuid = schild_id
                 , gtt.unix_time = present_time() ) );
    
    cache_.insert_node( id );
    cache_.insert_heading( id
                         , heading );
    cache_.insert_title( id
                       , title );

    return true;
}

auto Database::create_alias( Uuid const& src 
                           , Uuid const& dst )
    -> bool
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( src ) );
            BC_ASSERT( exists( dst ) );
            BC_ASSERT( !alias_exists( src
                                    , dst ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( alias_exists( src
                                   , dst ) );
        })
    ;

    auto at = aliases::aliases{};

    execute( insert_into( at )
           . set( at.src_uuid = to_string( src )
                , at.dst_uuid = to_string( dst ) ) );

    append_child_to_ordering( dst
                            , src );

    return true;
}

auto Database::path() const
    -> FsPath
{
    return path_;
}

auto Database::fetch_alias_destinations( Uuid const& src ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( src ) ); // TODO: Is this warranted, or should I just return {}?
        })
        BC_POST([ & ]
        {
            if( !rv.empty() )
            {
                BC_ASSERT( cache_.fetch_aliases().completed_set.count( src ) != 0 ); 
            }
        })
    ;

    auto const& cached = cache_.fetch_aliases();

    if( cached.completed_set.count( src ) != 0 )
    {
        auto const& srcs = cached.set.get< 1 >();

        for( auto [ it, end ] = srcs.equal_range( src )
           ; it != end
           ; ++it )
        {
            rv.emplace_back( it->second );
        }
    }
    else
    {
        auto at = aliases::aliases{};
        auto rows = execute( select( all_of( at ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
                           . from( at )
                           . where( at.src_uuid == to_string( src ) ) );

        for( auto const& e : rows )
        {
            rv.emplace_back( uuid_from_string( e.dst_uuid ).value() );

            cache_.insert_alias( src
                               , uuid_from_string( e.dst_uuid ).value() );
        }
    }

    return rv;
}

auto Database::fetch_alias_sources( Uuid const& dst ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( dst ) ); // TODO: Is this warranted, or should I just return {}?
        })
    ;

    auto const& cached = cache_.fetch_aliases();

    if( cached.completed_set.count( dst ) != 0 )
    {
        auto const& srcs = cached.set.get< 2 >();

        for( auto [ it, end ] = srcs.equal_range( dst )
           ; it != end
           ; ++it )
        {
            rv.emplace_back( it->first );
        }
    }
    else
    {
        auto at = aliases::aliases{};
        auto rows = execute( select( all_of( at ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
                                . from( at )
                                . where( at.dst_uuid == to_string( dst ) ) );
        for( auto const& e : rows )
        {
            rv.emplace_back( uuid_from_string( e.src_uuid ).value() );

            cache_.insert_alias( uuid_from_string( e.src_uuid ).value()
                               , dst );
        }
    }

    return rv;
}

auto Database::fetch_parent( Uuid const& id ) const
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_child( *rv
                                   , id ) );
            }
        })
    ;

    auto const& pmap = cache_.fetch_children().set.get< 2 >();

    if( auto const it = pmap.find( id )
      ; it != pmap.end() )
    {
        rv = it->first;
    }
    else if( !cache_.is_root( id ) )
    {
        auto const& sid = to_string( id );
        auto ct = children::children{};
        auto const& rt = execute( select( all_of( ct ) )
                                . from( ct )
                                . where( ct.child_uuid == sid ) );

        if( !rt.empty() )
        {
            // BC_ASSERT( rt.size() == 1 ); // TODO: Ensure each child has only a single parent.

            auto const parent = uuid_from_string( rt.front().parent_uuid ).value();

            // Force caching of all children of parent.
            fetch_children( parent );

            rv = parent;
        }
        else if( exists( id ) )
        {
            cache_.set_root( id );
        }
    }

    return rv;
}

auto Database::fetch_child( Heading const& heading
                          , Uuid const& parent ) const
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};
    auto const children =  fetch_children( parent );
    auto const headings = children
                        | views::transform( [ & ]( auto const& e ){ return std::make_pair( e, *fetch_heading( e ) ); } )
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
    -> Optional< std::string >
{
    auto rv = Optional< std::string >{};
    auto const& bodies = cache_.fetch_bodies();
    auto const& map = bodies.set.get< 1 >();

    if( auto const it = map.find( id )
      ; it != map.end() )
    {
        rv = it->second;
    }
    else // if( bodies.completed_set.count( id ) == 0 ) // Note: Should never be node without body.
    {
        auto const& sid = to_string( id );
        auto bt = bodies::bodies{};
        auto const& rt = execute( select( all_of( bt ) )
                                . from( bt )
                                . where( bt.uuid == sid ) );

        if( !rt.empty() )
        {
            rv = rt.front().body;

            cache_.insert_body( id
                              , *rv );
        }
    }

    return rv;
}

auto Database::fetch_heading( Uuid const& id ) const
    -> Optional< Heading >
{
    auto rv = Optional< Heading >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
    ;

    auto const& headings = cache_.fetch_headings();
    auto const& map = headings.set.get< 1 >();

    if( auto const it = map.find( id )
      ; it != map.end() )
    {
        rv = it->second; 
    }
    else// if( headings.completed_set.count( id ) == 0 ) // Note: Should never be node without heading.
    {
        auto const& sid = to_string( id );
        auto ht = headings::headings{};
        auto const& rt = execute( select( all_of( ht ) )
                                . from( ht )
                                . where( ht.uuid == sid ) );

        if( !rt.empty() )
        {
            rv = rt.front().heading;

            cache_.insert_heading( id
                                 , *rv );
        }
    }

    return rv;
}

auto Database::fetch_nodes( Heading const& heading ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const& headings = fetch_headings().get< 2 >();

    for( auto [ it, end ] = headings.equal_range( heading )
       ; it != end
       ; ++it )
    {
        rv.emplace( it->first );
    }

    return rv;
}

auto Database::fetch_headings() const
    -> UniqueIdMultiStrSet const&
{
    if( !cache_.fetch_headings().set_complete )
    {
        auto ht = headings::headings{};
        auto rows = execute( select( all_of( ht ) )
                           . from( ht )
                           . unconditionally() );
        
        for( auto const& e : rows )
        {
            cache_.insert_heading( uuid_from_string( e.uuid ).value()
                                 , e.heading );
        }

        cache_.mark_heading_set_complete();
    }

    return cache_.fetch_headings().set;
}

auto Database::fetch_bodies() const
    -> UniqueIdMultiStrSet const&
{
    if( !cache_.fetch_bodies().set_complete )
    {
        auto ht = bodies::bodies{};
        auto rows = execute( select( all_of( ht ) )
                           . from( ht )
                           . unconditionally() );
        
        for( auto const& e : rows )
        {
            cache_.insert_body( uuid_from_string( e.uuid ).value()
                              , e.body );
        }

        cache_.mark_body_set_complete();
    }

    return cache_.fetch_bodies().set;
}

auto Database::fetch_title( Uuid const& id ) const
    -> Optional< std::string >
{
    auto rv = Optional< std::string >{};
    auto const& titles = cache_.fetch_titles();
    auto const& map = titles.set.get< 1 >();

    if( auto const p = map.find( id )
      ; p != map.end() )
    {
        rv = p->second;
    }
    else// if( titles.completed_set.count( id ) == 0 ) // Note: Should never have node without title.
    {
        auto const& sid = to_string( id );
        auto tt = titles::titles{};
        auto const& rt = execute( select( all_of( tt ) )
                                . from( tt )
                                . where( tt.uuid == sid ) );

        if( !rt.empty() )
        {
            rv = rt.front().title;

            cache_.insert_title( id
                               , *rv );
        }
    }

    return rv;
}

auto Database::fetch_genesis_time( Uuid const& id ) const
    -> Optional< uint64_t >
{
    auto rv = Optional< uint64_t >{};
    auto const& sid = to_string( id );
    auto gtt = genesis_times::genesis_times{};
    auto const& rt = execute( select( all_of( gtt ) )
                            . from( gtt )
                            . where( gtt.uuid == sid ) );

    if( !rt.empty() )
    {
        rv = rt.front().unix_time;
    }

    return rv;
}

auto Database::fetch_nodes() const
    -> UuidUnSet const&
{
    if( auto const nodes = cache_.fetch_nodes()
      ; !nodes.set_complete )
    {
        auto nt = nodes::nodes{};
        auto rows = execute( select( all_of( nt ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
                           . from( nt )
                           . unconditionally() );

        for( auto const& e : rows )
        {
            cache_.insert_node( uuid_from_string( e.uuid ).value() );
        }

        cache_.mark_node_set_complete();
    }

    return cache_.fetch_nodes().set;
}

auto Database::heading_exists( Heading const& heading ) const
    -> bool
{
    auto const& headings = fetch_headings().get< 2 >();

    return headings.find( heading ) != headings.end();
}

auto Database::update_child_ordering( Uuid const& parent
                                    , std::vector< std::string > const& abbreviations )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
        BC_POST([ & ]
        {
        })
    ;

    auto const sparent = to_string( parent );
    auto const new_ordering = abbreviations
                            | views::join
                            | to< std::string >();

    {
        using sqlpp::sqlite3::insert_or_replace_into;

        auto ot = child_orderings::child_orderings{};

        execute( insert_or_replace_into( ot )
               . set( ot.parent_uuid = sparent
                    , ot.sequence = new_ordering ) );
    }
}

auto Database::update_heading( Uuid const& id
                             , Heading const& heading )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( *fetch_heading( id ) == heading );
        })
    ;

    auto ht = headings::headings{};

    execute( update( ht )
           . set( ht.heading = heading )
           . where( ht.uuid == to_string( id ) ) );
}

auto Database::update_title( Uuid const& id
                           , Title const& title )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( *fetch_title( id ) == title );
        })
    ;

    auto tt = titles::titles{};

    execute( update( tt )
           . set( tt.title = title )
           . where( tt.uuid == to_string( id ) ) );
}

auto Database::update_body( Uuid const& node
                          , std::string const& content )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( node ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( *fetch_body( node ) == content );
        })
    ;

    auto bt = bodies::bodies{};

    execute( update( bt )
           . set( bt.body = content )
           . where( bt.uuid == to_string( node ) ) );
}

auto Database::update_bodies( std::vector< std::pair< Uuid, std::string > > const& updates )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // TODO: For each, ensure node exists.
        })
        BC_POST([ & ]
        {
        })
    ;

    auto bt = bodies::bodies{};

    // TODO: There's a way to do this as a bulk statement (prepared statement?), rather than a loop.
    for( auto const& e : updates )
    {

        execute( update( bt )
               . set( bt.body = e.second )
               . where( bt.uuid == to_string( e.first ) ) );
    }
}

auto Database::exists( Uuid const& id ) const
    -> bool
{
    auto const& nodes = fetch_nodes();

    return nodes.find( id ) != nodes.end();
}

auto Database::alias_exists( Uuid const& src
                           , Uuid const& dst ) const
    -> bool
{
    auto at = aliases::aliases{};
    auto const& rv = execute( select( all_of( at ) )
                            . from( at )
                            . where( at.src_uuid == to_string( src )
                                  && at.dst_uuid == to_string( dst ) ) );

    return !rv.empty();
}

auto Database::is_child( Uuid const& parent
                       , Uuid const& id ) const
    -> bool
{
    auto const& children = fetch_children( parent );

    return children.find( id ) != children.end();
}

auto Database::is_child( Uuid const& parent
                       , Heading const& heading ) const
    -> bool
{
    return fetch_child( heading
                      , parent ).has_value();
}

auto Database::has_parent( Uuid const& child ) const
    -> bool
{
    return fetch_parent( child ).has_value();
}

auto Database::has_child_ordering( Uuid const& parent ) const
    -> bool
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
        })
    ;

    return !fetch_child_ordering( parent ).empty();
}

auto Database::is_ordered( Uuid const& parent
                         , Uuid const& child ) const
    -> bool
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( has_child_ordering( parent ) );
        })
    ;

    auto const oid = to_ordering_id( child );
    auto const os = fetch_child_ordering( parent );

    return end( os ) != find( os
                            , oid );
}

auto Database::remove( Uuid const& id )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
            BC_ASSERT( has_parent( id ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !exists( id ) );
            BC_ASSERT( fetch_child_ordering( id ).size() == fetch_children( id ).size() + fetch_alias_sources( id ).size() );
        })
    ;

    auto const parent = *fetch_parent( id );
    auto const alias_dsts = fetch_alias_destinations( id );

    {
        auto nt = nodes::nodes{};

        execute( remove_from( nt ) 
               . where( nt.uuid == to_string( id ) ) );
    }

    remove_from_ordering( parent
                        , id );

    { 
        auto const& src = id;

        for( auto const& dst : alias_dsts )
        {
            remove_from_ordering( src
                                , dst );
        }
    }
}

auto Database::fetch_children() const
    -> std::vector< std::pair< Uuid
                             , Uuid > >
{
    KMAP_PROFILE_SCOPE();

    auto rv = std::vector< std::pair< Uuid
                                    , Uuid > >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                exists( e.first );
                exists( e.second );
            }
        })
    ;

    // TODO: Return from cache.

    auto ct = children::children{};
    auto rows = execute( select( all_of( ct ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
                              . from( ct )
                              . unconditionally() );
    rv = [ & ] // Note: for some reason, views::transform doesn't seem compatible with 'rows'.
    {
        auto ps = std::vector< std::pair< Uuid
                                        , Uuid > >{};

        for( auto const& e : rows )
        {
            ps.emplace_back( uuid_from_string( e.parent_uuid ).value()
                           , uuid_from_string( e.child_uuid ).value() );
        }

        return ps;
    }();

    return rv;
}

auto Database::fetch_children( Uuid const& parent ) const
    -> UuidSet
{
    KMAP_PROFILE_SCOPE();

    auto rv = UuidSet{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) ); // TODO: Because this is a 'fetch' function, it's probably safe to omit this check and just return an empty vector to signify nothing found.
        })
    ;

    auto const& children = cache_.fetch_children();
    auto const& map = children.set.get< 1 >();

    if( children.completed_set.count( parent ) != 0 )
    {
        for( auto [ it, end ] = map.equal_range( parent )
           ; it != end
           ; ++it )
        {
            rv.emplace( it->second );
        }
    }
    else
    {
        auto ct = children::children{};
        auto rows = execute( select( all_of( ct ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
                                   . from( ct )
                                   . where( ct.parent_uuid == to_string( parent ) ) );
        auto const cids = [ & ] // Note: for some reason, views::transform doesn't seem compatible with 'rows'.
        {
            auto rv = UuidSet{};

            for( auto const& e : rows )
            {
                rv.emplace( uuid_from_string( e.child_uuid ).value() );
            }

            return rv;
        }();

        cache_.insert_children( parent
                              , cids );

        rv = cids;
    }

    return rv;
}

// TODO: rename get/fetch_headings
auto Database::child_headings( Uuid const& id ) const
    -> std::vector< Heading >
{
    auto rv = std::vector< Heading >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( fetch_children( id ).size() == rv.size() );
        })
    ;

    auto const children = fetch_children( id );

    rv = children
       | views::transform( [ & ]( auto const& e ){ return *fetch_heading( e ); } )
       | to< std::vector< Heading > >();

    return rv;
}

auto Database::remove_child( Uuid const& parent
                           , Uuid const& child )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
            BC_ASSERT( exists( child ) );
            BC_ASSERT( is_child( parent
                               , child ) );
            BC_ASSERT( is_ordered( parent
                                 , child ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !is_child( parent
                                , child ) );
            BC_ASSERT( !is_ordered( parent
                                  , child ) );
        })
    ;

    auto ct = children::children{};

    execute( remove_from( ct ) 
           . where( ct.parent_uuid == to_string( parent )
                 && ct.child_uuid == to_string( child ) ) );
    
    remove_from_ordering( parent
                        , child );
}

auto Database::remove_alias( Uuid const& src
                           , Uuid const& dst )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( src ) );
            BC_ASSERT( exists( dst ) );
            BC_ASSERT( alias_exists( src 
                                   , dst ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !alias_exists( src
                                    , dst ) );
        })
    ;

    remove_from_ordering( dst
                        , src );

    auto at = aliases::aliases{};

    execute( remove_from( at ) 
           . where( at.src_uuid == to_string( src )
                 && at.dst_uuid == to_string( dst ) ) );
}

auto Database::remove_from_ordering( Uuid const& parent
                                   , std::string const& order_id )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
        })
    ;

    auto const co = fetch_child_ordering( parent );
    auto const nco = co 
                   | views::remove( order_id )
                   | to_vector;

    KMAP_ASSERT_EQ( co.size(), nco.size() + 1 );

    update_child_ordering( parent
                         , nco );
}

auto Database::remove_alias_from_ordering( Uuid const& src
                                         , Uuid const& dst )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( src ) );
            BC_ASSERT( exists( dst ) );
            BC_ASSERT( has_child_ordering( src ) );
            BC_ASSERT( has_parent( dst ) );
            BC_ASSERT( alias_exists( src
                                   , dst ) );
        })
    ;

    remove_from_ordering( dst
                        , src );
}

auto Database::remove_from_ordering( Uuid const& parent
                                   , Uuid const& target )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
            BC_ASSERT( is_ordered( parent
                                 , target ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !is_ordered( parent
                                  , target ) );
        })
    ;

    remove_from_ordering( parent
                        , to_ordering_id( target ) );
}

auto Database::fetch_child_ordering( Uuid const& parent ) const
    -> std::vector< std::string >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
        })
    ;

    auto sorderings = [ & ]
    {
        auto const& orderings = cache_.fetch_child_orderings();
        auto const& map = orderings.set.get< 1 >();

        if( auto const it = map.find( parent )
          ; it != map.end() )
        {
            return it->second;
        }
        else if( orderings.completed_set.count( parent ) == 0 )
        {
            auto ot = child_orderings::child_orderings{};
            auto rt = execute( select ( all_of( ot ) )
                             . from( ot )
                             . where( ot.parent_uuid == to_string( parent ) ) );
            
            if( rt.empty() )
            {
                cache_.insert_child_orderings( parent
                                             , std::string{} );

                return std::string{}; // Parent has no children.
            }

            auto const seq = std::string{ rt.front().sequence };

            cache_.insert_child_orderings( parent
                                         , seq );
            
            return seq;
        }

        return std::string{};
    }();

    return sorderings
         | views::chunk( 8 )
         | to< StringVec >();
}

// TODO: What about the (highly unlikely) case that there's a conflict in the first 8 bytes of the UUID for child nodes?
auto Database::append_child_to_ordering( Uuid const& parent
                                       , Uuid const& child )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
            BC_ASSERT( exists( child ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( fetch_child_ordering( parent ).size() == fetch_children( parent ).size() + fetch_alias_sources( parent ).size() );
        })
    ;

    auto const sparent = to_string( parent );
    auto const new_ordering = [ & ] // TODO: Is this redunant with fetch_child_ordering + append? Or just slightly more efficient?
    {
        auto const schild = to_string( child );
        auto const appendage = schild
                             | views::take( 8 )
                             | to< std::string >();
        auto ot = child_orderings::child_orderings{};
        auto const rt = execute( select( all_of( ot ) )
                               . from( ot )
                               . where( ot.parent_uuid == sparent ) );
        if( rt.empty() )
        {
            return appendage;
        }
        auto const seq = std::string{ rt.front().sequence };

        return seq + appendage;
    }();

    {
        using sqlpp::sqlite3::insert_or_replace_into;

        auto ot = child_orderings::child_orderings{};

        execute( insert_or_replace_into( ot )
               . set( ot.parent_uuid = sparent
                    , ot.sequence = new_ordering ) );
    }
}

auto Database::flush_cache()
    -> void
{
    cache_.flush();
}

auto Database::execute_raw( std::string const& stmt )
    -> std::map< std::string, std::string >
{
    #if KMAP_LOGGING_DB_CACHE
    fmt::print( "[DEBUG] raw db const\n" );
    #endif // KMAP_LOGGING_DB_CACHE 

    flush_cache();

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

} // namespace kmap
