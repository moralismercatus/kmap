/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#if KMAP_LOGGING_ALL
    #define KMAP_LOGGING_DB
#endif // KMAP_LOGGING_PATH_ALL

#include <com/database/db.hpp>

#include <contract.hpp>
#include <com/database/table_decl.hpp>
#include <com/database/util.hpp>
#include <com/filesystem/filesystem.hpp> // TODO: only present until sql db moves to DatabaseFilesystem.
#include <emcc_bindings.hpp>
#include <error/db.hpp>
#include <error/filesystem.hpp>
#include <error/network.hpp>
#include <io.hpp>
#include <test/util.hpp>
#include <utility.hpp>
#include <util/result.hpp>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <boost/filesystem.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/reverse.hpp>
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
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/sqlite3/insert_or.h>
#include <sqlpp11/sqlpp11.h>

#include <functional>
#include <regex>

// TODO: TESTING
#include <fstream>

using namespace ranges;
namespace fs = boost::filesystem;
namespace sql = sqlpp::sqlite3;
namespace rvs = ranges::views;

namespace kmap::com {

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
    auto rv = result::make_result< void >();

    fmt::print( "[database] initializing\n" );

    rv = outcome::success();

    return rv;
}

auto Database::load()
    -> Result< void >
{
    fmt::print( "[database] loading\n" );
    return load_internal( kmap_inst().database_path() );
}

auto Database::load( FsPath const& path )
    -> Result< void >
{
    return load_internal( path );
}

// TODO: Shouldn't the load-from-db-file functionality actually be in DatabaseFilesystem?
auto Database::load_internal( FsPath const& path )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path.string() );

    KM_LOG_ST_DISABLE();

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( con_ );
                BC_ASSERT( fs::equivalent( path_, sqlite3_db_filename( con_->native_handle(), nullptr ) ) );
                fmt::print( "Datatbase::load_internal, post, has_delta: {}\n", has_delta() );
                BC_ASSERT( !has_delta() );
            }
        })
    ;

    path_ = path;

    KMAP_ENSURE( !path_.empty(), error_code::filesystem::file_not_found );
    KMAP_ENSURE( fs::exists( path ), error_code::filesystem::file_not_found );

    fmt::print( "Database :: load: {}\n", path_.string() );

#if KMAP_LOGGING_DB
    con_ = std::make_unique< sql::connection >( this->open_connection( path_, SQLITE_OPEN_READONLY, true ) );
#else
    con_ = std::make_unique< sql::connection >( this->open_connection( path_, SQLITE_OPEN_READONLY, false ) );
#endif

    {
        auto proc_table = [ & ]( auto&& table ) mutable
        {
            using namespace db;
            using Table = std::decay_t< decltype( table ) >;

            if constexpr( std::is_same_v< Table, db::NodeTable > )
            {
                auto t = nodes::nodes{};
                auto rows = execute( select( all_of( t ) )
                                   . from( t )
                                   . unconditionally() );

                for( auto const& e : rows )
                {
                    KTRYE( push_node( KTRYE( uuid_from_string( e.uuid ) ) ) );
                }
            }
            else if constexpr( std::is_same_v< Table, db::HeadingTable > )
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
            else if constexpr( std::is_same_v< Table, db::TitleTable > )
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
            else if constexpr( std::is_same_v< Table, db::BodyTable > )
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
            else if constexpr( std::is_same_v< Table, db::ChildTable > )
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
            else if constexpr( std::is_same_v< Table, db::AliasTable > )
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
            else if constexpr( std::is_same_v< Table, db::AttributeTable > )
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
            else if constexpr( std::is_same_v< Table, db::ResourceTable > )
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

            cache().apply_delta_to_cache< Table >();
        };

        boost::hana::for_each( boost::hana::reverse( cache().tables() ), proc_table );
        boost::hana::for_each( boost::hana::reverse( cache().tables() ), apply_delta );
    }

    {
#if KMAP_LOGGING_DB
        con_ = std::make_unique< sql::connection >( this->open_connection( path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, true ) );
#else
        con_ = std::make_unique< sql::connection >( this->open_connection( path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, false ) );
#endif
    }

    rv = outcome::success();

    return rv;
}

auto Database::init_db_on_disk( FsPath const& path )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path.string() );

    auto rv = result::make_result< void >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( con_ );
                BC_ASSERT( fs::equivalent( fs::absolute( path_ ), sqlite3_db_filename( con_->native_handle(), nullptr ) ) );
            }
        })
    ;

    path_ = path;

    if( path.extension().empty() )
    {
        path_.replace_extension( "kmap" );
    }

#if KMAP_LOGGING_DB
    con_ = std::make_unique< sql::connection >( this->open_connection( path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, true ) );
#else
    con_ = std::make_unique< sql::connection >( this->open_connection( path_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, false ) );
#endif

    // TODO: Is this necessary anymore, now that cascading is done outside of sqlite?
    // Sqlite3 requires that foriegn key support is enabled for each new connection.
    con_->execute
    (
        R"(
        PRAGMA foreign_keys = ON;
        )"
    );
    // TODO:
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

auto Database::cache()
    -> db::Cache&
{
    query_cache_.clear();

    return cache_;
}

auto Database::cache() const
    -> db::Cache const&
{
    return cache_;
}

auto Database::query_cache()
    -> db::QueryCache&
{
    return query_cache_;
}

auto Database::query_cache() const
    -> db::QueryCache const&
{
    return query_cache_;
}

auto Database::create_tables()
    -> void
{
    kmap::com::create_tables( *con_ );
}

// TODO: What about case where external actor overwrites db file? `con_` is still valid, and file still exists.
//       Would need exclusive ownership of file on disk?
auto Database::has_file_on_disk()
    -> bool
{
    auto const p = KTRYB( path() );

#if _DEBUG
    #warn False positive assertion in there is a bug in emscriptens ___syscall_newfstatat that is called by fs::exists. Recommend against \
          doing FS ops in DEBUG mode until its fixed. See https://github.com/emscripten-core/emscripten/issues/17660 for a related open ticket.
#endif // _DEBUG

    return !p.string().empty()
        && fs::exists( p )
        && static_cast< bool >( con_ )
        && fs::equivalent( p, sqlite3_db_filename( con_->native_handle(), nullptr ) );

    // TODO: SCENARIO( "external actor alters file on disk" )
}

auto Database::push_child( Uuid const& parent 
                         , Uuid const& child )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "parent", parent );
        // KM_RESULT_PUSH_NODE( "child", child );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // TODO: BC_ASSERT( is_child( parent, child ) );
        })
    ;

    KMAP_ENSURE( node_exists( parent ), error_code::network::invalid_node );
    KMAP_ENSURE( node_exists( child ), error_code::network::invalid_node ); 

    KTRY( cache().push< db::ChildTable >( db::Parent{ parent }, db::Child{ child } ) );
    // cache().push( TableId::attributes, child, db::AttributeValue{ fmt::format( "order:{}", 1 ) } );

    rv = outcome::success();

    return rv;
}

SCENARIO( "Database::push_child", "[benchmark][db]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database" );

    auto& km = kmap::Singleton::instance();
    auto const db = REQUIRE_TRY( km.fetch_component< com::Database >() );

    BENCHMARK( "gen_uuid" )
    {
        return gen_uuid();
    };
    BENCHMARK_ADVANCED( "std::map::insert" )( auto meter )
    {
        auto m = std::map< int, int >{};
        meter.measure( [ & ]{ return m.insert( { 1, 2 } ).second; } );
        m.erase( 1 );
    };
    BENCHMARK_ADVANCED( "push_node" )( auto meter )
    {
        auto const id = gen_uuid();
        meter.measure( [ & ]{ return db->push_node( id ); } );
        REQUIRE_TRY( db->erase_all( id ) );
    };
    BENCHMARK_ADVANCED( "push_node for 1000th node" )( auto meter )
    {
        auto ids = std::vector< Uuid >{};
        for( auto i = 0
           ; i < 1000
           ; ++i )
        {
            auto const id = gen_uuid();
            ids.emplace_back( id );
            REQUIRE_TRY( db->push_node( id ) );
        }
        {
            auto const id = gen_uuid();
            meter.measure( [ & ]{ return db->push_node( id ); } );
            REQUIRE_TRY( db->erase_all( id ) );
        }
        for( auto const& id : ids )
        {
            REQUIRE_TRY( db->erase_all( id ) );
        }
    };
}

auto Database::push_node( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", to_string( id ) );

    KMAP_ENSURE( !node_exists( id ), error_code::network::duplicate_node );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cache().push< db::NodeTable >( id ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_heading( Uuid const& node
                           , std::string const& heading )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", to_string( node ) );
        KM_RESULT_PUSH( "heading", heading );

    KMAP_ENSURE( node_exists( node ), error_code::common::uncategorized );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cache().push< db::HeadingTable >( node, heading ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_body( Uuid const& node
                        , std::string const& body )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", to_string( node ) );
        KM_RESULT_PUSH( "body", body );

    KMAP_ENSURE( node_exists( node ), error_code::common::uncategorized );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cache().push< db::BodyTable >( node, body ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_title( Uuid const& node
                         , std::string const& title )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", to_string( node ) );
        KM_RESULT_PUSH( "title", title );

    KMAP_ENSURE( node_exists( node ), error_code::common::uncategorized );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cache().push< db::TitleTable >( node, title ) );

    rv = outcome::success();

    return rv;
}

auto Database::push_attr( Uuid const& parent
                        , Uuid const& attr )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "parent", to_string( parent ) );
        KM_RESULT_PUSH( "attr", to_string( attr ) );

    KMAP_ENSURE( node_exists( parent ), error_code::common::uncategorized );

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cache().push< db::AttributeTable >( db::Left{ parent }, db::Right{ attr } ) );

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
    // cache().push( db::TableId::nodes
    //            , id
    //            , db::NodeValue{} );
    // cache().push( db::TableId::headings
    //            , id
    //            , db::HeadingValue{ heading } );
    // cache().push( db::TableId::titles
    //            , id
    //            , db::TitleValue{ title } );

//     return true;
// }

auto Database::push_alias( Uuid const& src 
                         , Uuid const& dst )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src", to_string( src ) );
        KM_RESULT_PUSH( "dst", to_string( dst ) );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( alias_exists( src, dst ) );
            }
        })
    ;

    KMAP_ENSURE( node_exists( src ), error_code::network::invalid_node );
    KMAP_ENSURE( node_exists( dst ), error_code::network::invalid_node );
    KMAP_ENSURE( !alias_exists( src, dst ), error_code::network::invalid_node );

    KTRY( cache().push< db::AliasTable >( db::Left{ src }, db::Right{ dst } ) );

    rv = outcome::success();

    return rv;
}

auto Database::path() const
    -> Result< FsPath >
{
    KM_RESULT_PROLOG();

    if( path_.empty() )
    {
        return KMAP_MAKE_ERROR( error_code::filesystem::file_not_found );
    }
    else
    {
        return path_;
    }
}

auto Database::open_connection( FsPath const& db_path
                              , int flags
                              , bool debug )
    -> sqlpp::sqlite3::connection
{
    // TODO: Here post-open, either set document tile, or fire event. For now, it would be enough to set title. Is event system always live by this time?
    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        KM_RESULT_PROLOG();

        auto const& estorev = estore.value();

        KTRYE( estorev->fire_event( { "subject.database", "verb.opened", "object.database_connection" } ) );
    }

    return db::open_connection( db_path, flags, debug );
}

auto Database::fetch_alias_destinations( Uuid const& src ) const
    -> Result< std::vector< Uuid > >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "src", src );

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
                // BC_ASSERT( cache().fetch_aliases().completed_set.count( src ) != 0 ); 
            }
        })
    ;

    auto r = cache().fetch_values< db::AliasTable >( db::Left{ src } );

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

    // auto const& cached = cache().fetch_aliases();

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

    //         cache().insert_alias( src
    //                            , uuid_from_string( e.dst_uuid ).value() );
    //     }
    // }

    return rv;
}

auto Database::fetch_alias_sources( Uuid const& dst ) const
    -> std::vector< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "dst", dst );

    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( dst ) ); // TODO: Is this warranted, or should I just return {}?
        })
    ;

    KMAP_THROW_EXCEPTION_MSG( "Impl. needed" );

    // auto const& cached = cache().fetch_aliases();

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

    //         cache().insert_alias( uuid_from_string( e.src_uuid ).value()
    //                            , dst );
    //     }
    // }

    return rv;
}

auto Database::fetch_parent( Uuid const& id ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", id );

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

    auto const parent = KTRY( cache().fetch_values< db::ChildTable >( db::Child{ id } ) );

    BC_ASSERT( parent.size() == 1 );

    rv = parent.at( 0 ).value();
    
    return rv;
}

// TODO: This probably doesn't belong in Database. It's a utility.
auto Database::fetch_child( Uuid const& parent 
                          , Heading const& heading ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_STR( "heading", heading );

    KMAP_ENSURE_EXCEPT( node_exists( parent ) ); // TODO: replace with ensure_result

    // TODO: This assumes that every node has a heading. Is this constraint enforced? The only way would be to force Database::create_node( id, heading );
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const children = KTRY( fetch_children( parent ) );
    auto const headings = children
                        | views::transform( [ & ]( auto const& e ){ return std::make_pair( e, KTRYE( fetch_heading( e ) ) ); } )
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
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", id );

    KMAP_ENSURE_EXCEPT( node_exists( id ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( std::string );

    rv = KTRY( cache().fetch_value< db::BodyTable >( id ) );

    return rv;
}

auto Database::fetch_heading( Uuid const& id ) const
    -> Result< Heading >
{
    KM_RESULT_PROLOG();

    KMAP_ENSURE_EXCEPT( node_exists( id ) ); // TODO: replace with ensure_result

    auto rv = KMAP_MAKE_RESULT( Heading );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( id ) );
        })
    ;

    rv = KTRY( cache().fetch_value< db::HeadingTable >( id ) );

    return rv;
}

auto Database::fetch_attr( Uuid const& id ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", id );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( node_exists( id ), error_code::network::invalid_node );

    BC_CONTRACT()
        BC_POST([ & ]
        {
        })
    ;

    // TODO: attr should only ever 1 parent, so the table type should represent this.
    auto const attr = KTRY( cache().fetch_values< db::AttributeTable >( db::Left{ id } ) );

    if( attr.size() == 1 )
    {
        rv = attr.at( 0 ).value();
    }

    return rv;
}

auto Database::fetch_attr_owner( Uuid const& attrn ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "attr", attrn );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    
    KMAP_ENSURE( node_exists( attrn ), error_code::network::invalid_node );

    // TODO: attr should only ever 1 parent, so the table type should represent this.
    auto const attr = KTRY( cache().fetch_values< db::AttributeTable >( db::Right{ attrn } ) );

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
    // if( !cache().fetch_bodies().set_complete )
    // {
    //     auto ht = bodies::bodies{};
    //     auto rows = execute( select( all_of( ht ) )
    //                        . from( ht )
    //                        . unconditionally() );
        
    //     for( auto const& e : rows )
    //     {
    //         cache().insert_body( uuid_from_string( e.uuid ).value()
    //                           , e.body );
    //     }

    //     cache().mark_body_set_complete();
    // }

    // return cache().fetch_bodies().set;
//     return {};
// }

auto Database::fetch_title( Uuid const& id ) const
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", id );

    auto rv = KMAP_MAKE_RESULT( std::string );

    rv = KTRY( cache().fetch_value< db::TitleTable >( id ) );

    // auto const& titles = cache().fetch_titles();
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

    //         cache().insert_title( id
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
    // if( auto const nodes = cache().fetch_nodes()
    //   ; !nodes.set_complete )
    // {
    //     auto nt = nodes::nodes{};
    //     auto rows = execute( select( all_of( nt ) ) // Note: must be non-const b/c it's like istream: state changes on reads.
    //                        . from( nt )
    //                        . unconditionally() );

    //     for( auto const& e : rows )
    //     {
    //         cache().insert_node( uuid_from_string( e.uuid ).value() );
    //     }

    //     cache().mark_node_set_complete();
    // }

    // return cache().fetch_nodes().set;
    return {};
}

auto Database::update_heading( Uuid const& node
                             , Heading const& heading )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", node );
        KM_RESULT_PUSH_STR( "heading", heading );

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

    KTRY( cache().push< db::HeadingTable >( node, heading ) );

    rv = outcome::success();

    return rv;
}

auto Database::update_title( Uuid const& node
                           , Title const& title )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", node );
        KM_RESULT_PUSH_STR( "title", title );

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

    KTRY( cache().push< db::TitleTable >( node, title ) );

    rv = outcome::success();

    return rv;
}

auto Database::update_body( Uuid const& node
                          , std::string const& content )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", node );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( node_exists( node ), error_code::network::invalid_node );

    KTRY( cache().push< db::BodyTable >( node, content ) );

    rv = outcome::success();

    return rv;
}

auto Database::node_exists( Uuid const& id ) const
    -> bool
{
    return !cache().contains_erased_delta< db::NodeTable >( id )
        && ( cache().contains_cached< db::NodeTable >( id )
          || cache().contains_delta< db::NodeTable >( id ) );
}

auto Database::attr_exists( Uuid const& id ) const
    -> bool
{
    auto const key = db::Child{ id };

    return !cache().contains_erased_delta< db::AttributeTable >( key )
        && ( cache().contains_cached< db::AttributeTable >( key )
          || cache().contains_delta< db::AttributeTable >( key ) );
}

auto Database::alias_exists( Uuid const& src
                           , Uuid const& dst ) const
    -> bool
{
    auto const key = db::AliasTable::unique_key_type{ db::Src{ src }, db::Dst{ dst } };

    return !cache().contains_erased_delta< db::AliasTable >( key )
        && ( cache().contains_cached< db::AliasTable >( key )
          || cache().contains_delta< db::AliasTable >( key ) );
}

auto Database::is_child( Uuid const& parent
                       , Uuid const& id ) const
    -> bool
{
    KM_RESULT_PROLOG();

    auto const& children = KTRYE( fetch_children( parent ) ); // TODO: Replace with KMAP_TRY().

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
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

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
                KTRYE( cache().erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::HeadingTable > )
        {
            if( contains< Table >( id ) )
            {
                KTRYE( cache().erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::TitleTable > )
        {
            if( contains< Table >( id ) )
            {
                KTRYE( cache().erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::BodyTable > )
        {
            if( cache().contains< Table >( id ) )
            {
                KTRYE( cache().erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::ResourceTable > )
        {
            if( cache().contains< Table >( id ) )
            {
                KTRYE( cache().erase< Table >( id ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::AttributeTable > )
        {
            if( auto const parent = fetch_attr_owner( id )
              ; parent )
            {
                KTRYE( cache().erase< Table >( db::Parent{ parent.value() }, db::Child{ id } ) );
            }
        }
        else if constexpr( std::is_same_v< Table, db::ChildTable > )
        {
            if( auto const parent = fetch_parent( id )
              ; parent )
            {
                KTRYE( cache().erase< Table >( db::Parent{ parent.value() }, db::Child{ id } ) );
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
                    KTRYE( cache().erase< Table >( db::Src{ id }, db::Dst{ dst } ) );
                }
            }
        }
        else
        {
            static_assert( always_false< Table >::value, "non-exhaustive visitor!" );
        }
    };

    boost::hana::for_each( cache().tables(), fn );

    rv = outcome::success();

    return rv;
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "parent", parent );

    auto rv = KMAP_MAKE_RESULT( UuidSet );

    BC_CONTRACT()
        BC_POST([ & ]
        {
        })
    ;

    KMAP_ENSURE( node_exists( parent ), error_code::network::invalid_node );

    if( auto const vs = cache().fetch_values< db::ChildTable >( db::Parent{ parent } )
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
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "parent", parent );
        // KM_RESULT_PUSH_NODE( "child", child );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node_exists( parent ) );
            BC_ASSERT( node_exists( child ) );
            BC_ASSERT( is_child( parent, child ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !is_child( parent, child ) );
            }

            BC_ASSERT( node_exists( parent ) );
            BC_ASSERT( node_exists( child ) );
        })
    ;

    KMAP_ENSURE( node_exists( parent ), error_code::network::invalid_node );
    KMAP_ENSURE( node_exists( child ), error_code::network::invalid_node );
    KMAP_ENSURE( is_child( parent, child ), error_code::network::invalid_parent );

    KTRY( cache().erase< db::ChildTable >( db::Parent{ parent }, db::Child{ child } ) );
    
    rv = outcome::success();

    return rv;
}

auto Database::erase_alias( Uuid const& src
                          , Uuid const& dst )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "src", src );
        // KM_RESULT_PUSH_NODE( "dst", dst );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( alias_exists( src, dst ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !alias_exists( src, dst ) );
            }

            BC_ASSERT( node_exists( src ) );
            BC_ASSERT( node_exists( dst ) );
        })
    ;

    KMAP_ENSURE( node_exists( src ), error_code::network::invalid_node );
    KMAP_ENSURE( node_exists( dst ), error_code::network::invalid_node );
    KMAP_ENSURE( alias_exists( src, dst ), error_code::network::invalid_alias );

    KTRY( cache().erase< db::AliasTable >( db::Src{ src }, db::Dst{ dst } ) );
    
    rv = outcome::success();

    return rv;
}

// TODO: This should probably be part of db_fs, and not db proper.
auto Database::flush_delta_to_disk()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( con_ );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !has_delta() );
            }
        })
    ;

    KMAP_ENSURE( has_file_on_disk(), error_code::common::uncategorized );

    if( has_delta() )
    {
        fmt::print( "[kmap][log][db] flush_delta_to_disk: has_delta, flushing...\n" );
    }

    auto proc_table = [ & ]( auto&& table ) mutable
    {
        using namespace db;
        using sqlpp::sqlite3::insert_or_replace_into;
        // using sqlpp::dynamic_remove_from;
        using sqlpp::remove_from;
        using sqlpp::parameter;
        using Table = std::decay_t< decltype( table ) >;

        auto delta_pushed = false;
        auto nt = nodes::nodes{};
        auto nt_ins = insert_into( nt ).columns( nt.uuid ); // Nodes should never be "replaced"/updated.
        auto nt_rm = con_->prepare( remove_from( nt ).where( nt.uuid == parameter( nt.uuid ) ) );
        auto ht = headings::headings{};
        auto ht_ins = insert_or_replace_into( ht ).columns( ht.uuid, ht.heading );
        auto ht_rm = con_->prepare( remove_from( ht ).where( ht.uuid == parameter( ht.uuid ) ) );
        auto tt = titles::titles{};
        auto tt_ins = insert_or_replace_into( tt ).columns( tt.uuid, tt.title );
        auto tt_rm = con_->prepare( remove_from( tt ).where( tt.uuid == parameter( tt.uuid ) ) );
        auto bt = bodies::bodies{};
        auto bt_ins = insert_or_replace_into( bt ).columns( bt.uuid, bt.body );
        auto bt_rm = con_->prepare( remove_from( bt ).where( bt.uuid == parameter( bt.uuid ) ) );
        auto ct = children::children{};
        auto ct_ins = insert_into( ct ).columns( ct.parent_uuid, ct.child_uuid );
        auto ct_rm = con_->prepare( remove_from( ct ).where( ct.parent_uuid == parameter( ct.parent_uuid ) && ct.child_uuid == parameter( ct.child_uuid ) ) );
        auto at = aliases::aliases{};
        auto at_ins = insert_into( at ).columns( at.src_uuid, at.dst_uuid );
        auto at_rm = con_->prepare( remove_from( at ).where( at.src_uuid == parameter( at.src_uuid ) && at.dst_uuid == parameter( at.dst_uuid ) ) );
        auto att = attributes::attributes{};
        auto att_ins = insert_into( att ).columns( att.parent_uuid, att.child_uuid );
        auto att_rm = con_->prepare( remove_from( att ).where( att.parent_uuid == parameter( att.parent_uuid ) && att.child_uuid == parameter( att.child_uuid ) ) );

        for( auto const& item : table )
        {
            if constexpr( std::is_same_v< Table, NodeTable > )
            {
                if( auto const& dis = item.delta_items
                  ; !dis.empty() )
                {
                    if( dis.back().action == DeltaType::erased )
                    {
                        nt_rm.params.uuid = to_string( item.key() );
                        ( *con_ )( nt_rm );
                        delta_pushed = true;
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
                        ht_rm.params.uuid = to_string( item.key() );
                        ( *con_ )( ht_rm );
                        delta_pushed = true;
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
                        tt_rm.params.uuid = to_string( item.key() );
                        ( *con_ )( tt_rm );
                        delta_pushed = true;
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
                        bt_rm.params.uuid = to_string( item.key() );
                        ( *con_ )( bt_rm );
                        delta_pushed = true;
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
                        ct_rm.params.parent_uuid = to_string( item.left().value() );
                        ct_rm.params.child_uuid = to_string( item.right().value() );
                        ( *con_ )( ct_rm );
                        delta_pushed = true;
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
                        at_rm.params.src_uuid = to_string( item.left().value() );
                        at_rm.params.dst_uuid = to_string( item.right().value() );
                        ( *con_ )( at_rm );
                        delta_pushed = true;
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
                        att_rm.params.parent_uuid = to_string( item.left().value() );
                        att_rm.params.child_uuid = to_string( item.right().value() );
                        ( *con_ )( att_rm );
                        delta_pushed = true;
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
             || delta_pushed )
            {
                cache().apply_delta_to_cache< Table >();
            }
            if( !ins.values._data._insert_values.empty() )
            {
                execute( ins );
            }
            // if( !nt_rm.where._data._dynamic_expressions.empty() )
            // {
            //     execute( rm );
            // }
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

    boost::hana::for_each( cache().tables(), proc_table );

    rv = outcome::success();

    return rv;
}

// TODO: This should probably be part of db_fs, and not db proper.
auto Database::flush_cache_to_disk()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( con_ );
        })
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
        using Table = std::decay_t< decltype( table ) >;

        auto nt = nodes::nodes{};
        auto nt_ins = insert_into( nt ).columns( nt.uuid );
        auto ht = headings::headings{};
        auto ht_ins = insert_into( ht ).columns( ht.uuid, ht.heading );
        auto tt = titles::titles{};
        auto tt_ins = insert_into( tt ).columns( tt.uuid, tt.title );
        auto bt = bodies::bodies{};
        auto bt_ins = insert_into( bt ).columns( bt.uuid, bt.body );
        auto ct = children::children{};
        auto ct_ins = insert_into( ct ).columns( ct.parent_uuid, ct.child_uuid );
        auto at = aliases::aliases{};
        auto at_ins = insert_into( at ).columns( at.src_uuid, at.dst_uuid );
        auto att = attributes::attributes{};
        auto att_ins = insert_into( att ).columns( att.parent_uuid, att.child_uuid );

        for( auto const& item : table )
        {
            if constexpr( std::is_same_v< Table, NodeTable > ) { nt_ins.values.add( nt.uuid = to_string( item.key() ) ); }
            else if constexpr( std::is_same_v< Table, HeadingTable > ) { ht_ins.values.add( ht.uuid = to_string( item.left() ), ht.heading = item.right() ); }
            else if constexpr( std::is_same_v< Table, TitleTable > ) { tt_ins.values.add( tt.uuid = to_string( item.left() ), tt.title = item.right() ); }
            else if constexpr( std::is_same_v< Table, BodyTable > ) { bt_ins.values.add( bt.uuid = to_string( item.left() ), bt.body = item.right() ); }
            else if constexpr( std::is_same_v< Table, ChildTable > ) { ct_ins.values.add( ct.parent_uuid = to_string( item.left().value() ), ct.child_uuid = to_string( item.right().value() ) ); }
            else if constexpr( std::is_same_v< Table, AliasTable > ) { at_ins.values.add( at.src_uuid = to_string( item.left().value() ) , at.dst_uuid = to_string( item.right().value() ) ); }
            else if constexpr( std::is_same_v< Table, AttributeTable > ) { att_ins.values.add( att.parent_uuid = to_string( item.left().value() ) , att.child_uuid = to_string( item.right().value() ) ); }
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

        auto push_to_db = [ & ]( auto const& ins ) mutable
        {
            if( !ins.values._data._insert_values.empty() )
            {
                execute( ins );
            }
        };

        if constexpr( std::is_same_v< Table, NodeTable > ) { push_to_db( nt_ins ); }
        else if constexpr( std::is_same_v< Table, HeadingTable > ) { push_to_db( ht_ins ); }
        else if constexpr( std::is_same_v< Table, TitleTable > ) { push_to_db( tt_ins ); }
        else if constexpr( std::is_same_v< Table, BodyTable > ) { push_to_db( bt_ins ); }
        else if constexpr( std::is_same_v< Table, ChildTable > ) { push_to_db( ct_ins ); }
        else if constexpr( std::is_same_v< Table, AliasTable > ) { push_to_db( at_ins ); }
        else if constexpr( std::is_same_v< Table, AttributeTable > ) { push_to_db( att_ins ); }
        else if constexpr( std::is_same_v< Table, ResourceTable > ) { /*TODO*/ }
        else { static_assert( always_false< Table >::value, "non-exhaustive visitor!" ); }
    };
    auto apply_delta = [ & ]( auto&& table ) mutable
    {
        using Table = std::decay_t< decltype( table ) >;

        // Only apply delta after write is complete.
        cache().apply_delta_to_cache< Table >();
    };

    boost::hana::for_each( cache().tables(), proc_table );
    boost::hana::for_each( cache().tables(), apply_delta );

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

    boost::hana::for_each( cache().tables(), fn );

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
//     // return cache().fetch( tbl, key );
// }

// auto Database::push( TableId const& tbl
//                    , db::Key const& id
//                    , ItemValue const& value )
//     -> Result< void >
// {
//     return cache().push( tbl, id, value );
// }

// TODO: I don't know if execute_raw should even be public... very dangerous, as the cache isn't reflected.
auto Database::execute_raw( std::string const& stmt )
    -> std::multimap< std::string, std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "stmt", stmt );

    KMAP_ENSURE_EXCEPT( has_file_on_disk() );

    KTRYE( flush_delta_to_disk() );

    return kmap::com::execute_raw( *con_, stmt );
}

auto create_attr_node( Database& db
                     , Uuid const& parent )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "parent", parent );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const mn = gen_uuid();

    KTRY( db.push_node( mn ) );
    KTRY( db.push_heading( mn, "$" ) );
    KTRY( db.push_title( mn, "$" ) );
    KTRY( db.push_attr( parent, mn ) );

    rv = mn;

    return rv;
}

auto create_tables( sqlpp::sqlite3::connection& con )
    -> void
{
    // Note: "IF NOT EXISTS" is used to allow create_tables to be called for
    // each connection, in case the database does not contain a new table that
    // has been added after original database creation.

    for( auto const& sql : db::table_map | rvs::values )
    {
        con.execute( sql );
    }
}

auto execute_raw( sqlpp::sqlite3::connection& con
                , std::string const& stmt )
    -> std::multimap< std::string, std::string >
{
    auto rv = std::multimap< std::string, std::string >{};
    auto const handle = con.native_handle();
    char* err_msg = {};
    auto const callback = []( void* output
                            , int argc
                            , char** argv
                            , char** name )
    {
        auto& sout = *static_cast< std::multimap< std::string, std::string >* >( output );

        for( auto i = 0
           ; i < argc
           ; ++i )
        {
            BC_ASSERT( name[ i ] != nullptr );

            sout.emplace( std::string{ name[ i ] }
                        , std::string{ argv[ i ] == nullptr ? "NULL" : argv[ i ] } );
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
