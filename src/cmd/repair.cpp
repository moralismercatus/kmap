/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "repair.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../db.hpp"
#include "../emcc_bindings.hpp"
#include "../error/filesystem.hpp"
#include "../error/master.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

#include <boost/filesystem.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>

#include <map>

using namespace ranges;
namespace fs = boost::filesystem;

namespace kmap::cmd::repair {

namespace {

template< typename CheckFn
        , typename RepairFn >
auto repair_missing_entry( Database& db
                         , std::string const& entry_type
                         , CheckFn check
                         , RepairFn repair )
    -> void
{
    auto const& nt = db.fetch< db::NodeTable >();

    for( auto const& ti : nt )
    {
        auto const nid = ti.key();

        if( !check( nid ) )
        {
            fmt::print( "[repair.state] Repairing missing {} entry for: {}\n"
                      , entry_type
                      , to_string( nid ) );
            
            repair( nid );
        }
    }
}

} // namespace anon

/**
 * Searches for discrepancies between children and orderings and attempts to repair.
**/
auto repair_orderings( Database& db )
    -> void
{
    KMAP_THROW_EXCEPTION_MSG( "TODO: Impl." );

#if 0
    for( auto const pid : db.fetch_nodes() )
    {
        auto const children = [ & ]
        {
            auto const rcids = KMAP_TRYE( db.fetch_children( pid ) );
            auto const ass = db.fetch_alias_sources( pid );

            return views::concat( rcids
                               , ass )
                 | to_vector;
        }();
        auto cos = db.fetch_child_ordering( pid );
        auto const child_map = children
                             | views::transform( [ & ]( auto const& e )
                             {
                                 return std::make_pair( to_ordering_id( e )
                                                      , e );
                             } )
                             | to< std::map< std::string, Uuid > >();
        auto cabbrevs = child_map
                      | views::keys
                      | to_vector;
        sort( cos );
        sort( cabbrevs );
        auto const missing_ordering = views::set_difference( cabbrevs
                                                          , cos )
                                    | to_vector;
        auto const missing_children = views::set_difference( cos
                                                          , cabbrevs );
        
        for( auto const& e : missing_ordering )
        {
            fmt::print( "[repair.state] Repairing child ordering for: [ {}, {} ]\n"
                        , to_string( pid )
                        , db.fetch_heading( pid ).value() );
            
            db.append_child_to_ordering( pid
                                       , child_map.at( e ) );
        }
        for( auto const& e : missing_children )
        {
            fmt::print( "[repair.state] Repairing child ordering for: [ {}, {} ]\n"
                        , to_string( pid )
                        , db.fetch_heading( pid ).value() );
            
            db.remove_from_ordering( pid
                                   , e );
        }
    }
#endif // 0
}

// TODO: Account for aliases. This does not account for child alias headings that conflict.
/**
 * Searches all nodes for conflicting heading paths, and attempts to resolve the conflict.
**/
auto repair_conflicting_headings( Database& db )
    -> void
{
    for( auto const pid : db.fetch_nodes() )
    {
        auto const cids = KMAP_TRYE( db.fetch_children( pid ) );
        auto chs = cids
                 | views::transform( [ & ]( auto const& e ){ return std::make_pair( e, db.fetch_heading( e ).value() ); } )
                 | to_vector;

        for( auto it1 = chs.begin()
           ; it1 != chs.end()
           ; ++it1 )
        {
            for( auto it2 = chs.cbegin()
            ; it2 != chs.end()
            ; ++it2 )
            {
                if( it1 != it2
                 && it1->second == it2->second )
                {
                    fmt::print( "[repair.state] Repairing conflicting child heading for: [ {}, {} ]\n"
                              , db.fetch_heading( it1->first ).value()
                              , db.fetch_heading( it2->first ).value() );
                    
                    auto const new_heading = fmt::format( "{}_conflict", db.fetch_heading( it2->first ).value() );

                    KMAP_TRYE( db.update_heading( it2->first, new_heading ) );
                    it1->second = new_heading;
                }
            }
        }
    }
}

auto back_up_state( FsPath const& fp )
    -> void
{
    auto const bkup_fp = FsPath{ fp }.replace_extension( ".pre-repair" );

    fmt::print( "Backing up before performing repairs: {}...\n"
                , bkup_fp.string() );
    
    fs::copy_file( fp
                 , bkup_fp
                 , fs::copy_option::overwrite_if_exists );
}

auto check_state( Database const& db )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    // Check that each table record maps to an entry in the "nodes" table.
    // check_nodes_table_mappings( "headings" );
    // check_node_table_mappings( "titles" );
    auto const all_nodes = [ & ]
    {
        auto nodes = nodes::nodes{};
        auto rv = std::set< std::string >{};
        for( auto rows = db.execute( select( all_of( nodes ) )
                                   . from( nodes )
                                   . unconditionally() )
           ; auto const& e : rows )
        {
            rv.emplace( std::string{ e.uuid } );
        }
        return rv;
    }();
    auto const [ all_parents, all_children ] = [ & ]
    {
        auto children = children::children{};
        auto p = std::set< std::string >{};
        auto c = std::set< std::string >{};
        for( auto rows = db.execute( select( all_of( children ) )
                                   . from( children )
                                   . unconditionally() )
           ; auto const& e : rows )
        {
            p.emplace( std::string{ e.parent_uuid } );
            c.emplace( std::string{ e.child_uuid } );
        }
        return std::pair{ p, c };
    }();

    auto check_single = [ & ]( auto const tbl
                             , auto const entry_name )
    {
        io::print( "[log] checking: {}\n", entry_name );
        for( auto rows = db.execute( select( all_of( tbl ) ).from( tbl ).unconditionally() )
           ; auto const& e : rows )
        {
            if( !( all_nodes.contains( std::string{ e.uuid } ) ) )
            {
                io::print( "abnormality detected: ({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.uuid } );
            }
        }
    };

    check_single( headings::headings{}, "heading" );
    check_single( titles::titles{}, "title" );
    check_single( bodies::bodies{}, "body" );
    check_single( resources::resources{}, "resource" );

    {
        io::print( "[log] checking: children\n" );

        auto tbl = children::children{};

        for( auto rows = db.execute( select( all_of( tbl ) ).from( tbl ).unconditionally() )
           ; auto const& e : rows )
        {
            if( !( all_nodes.contains( std::string{ e.parent_uuid } ) ) )
            {
                io::print( "abnormality detected: child:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.parent_uuid } );
            }
            if( !( all_nodes.contains( std::string{ e.child_uuid } ) ) )
            {
                io::print( "abnormality detected: child:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.child_uuid } );
            }
        }
    }
    {
        io::print( "[log] checking: aliases\n" );

        auto tbl = aliases::aliases{};

        for( auto rows = db.execute( select( all_of( tbl ) ).from( tbl ).unconditionally() )
           ; auto const& e : rows )
        {
            if( !( all_nodes.contains( std::string{ e.src_uuid } ) || all_nodes.contains( std::string{ e.dst_uuid } ) ) )
            {
                io::print( "abnormality detected: alias:({},{}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.src_uuid }
                         , std::string{ e.dst_uuid } );
            }
        }
    }
    {
        io::print( "[log] checking: child_ordering\n" );

        KMAP_THROW_EXCEPTION_MSG( "TODO: Impl." );
        // auto tbl = child_orderings::child_orderings{};

        // for( auto rows = db.execute( select( all_of( tbl ) ).from( tbl ).unconditionally() )
        //    ; auto const& e : rows )
        // {
        //     if( !( all_nodes.contains( std::string{ e.parent_uuid } ) ) )
        //     {
        //         io::print( "abnormality detected: child_ordering:({}) does not correspond to any entry in nodes table\n"
        //                  , std::string{ e.parent_uuid } );
        //     }
        // }
    }
    {
        io::print( "[log] checking: for unbegotten nodes. Should only be a single unbegotten (root)\n" );
        for( auto const& e : all_parents )
        {
            if( !all_children.contains( e ) )
            {
                io::print( "[log] unbegotten node: {}\n", e );
            }
        }
    }

    rv = outcome::success();

    return rv;
}

auto check_state( FsPath const& path )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    auto const fp = kmap_root_dir / path;

    if( file_exists( fp ) )
    {
        auto db = Database{};

        KMAP_TRYE( db.init_db_on_disk( fp ) );

        rv = check_state( db ); 
    }

    return rv;
}

auto repair_state( FsPath const& path )
    -> Result< void >
{   
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    auto const fp = kmap_root_dir / path;

    if( file_exists( fp ) )
    {
        back_up_state( fp );

        auto db = Database{};

        KMAP_TRYE( db.init_db_on_disk( fp ) );

        db.create_tables();

        // Note: Using `/meta.repair` for generating missing nodes.
        // repair_orphaned_children( db );
        fmt::print( "Analyzing orderings...\n" );
        repair_orderings( db );
        fmt::print( "Analyzing missing entries...\n" );
        repair_missing_entry( db
                            , "heading"
                            , [ &db ]( auto const& e ) -> bool { return db.fetch_heading( e ).has_value(); }
                            , [ &db ]( auto const& e ){ auto t = headings::headings{}; 
                                                        db.execute( insert_into( t ).set( t.uuid = to_string( e ), t.heading = format_heading( to_string( e ) ) ) ); } );
        repair_missing_entry( db
                            , "title"
                            , [ &db ]( auto const& e ) -> bool { return db.fetch_title( e ).has_value(); }
                            , [ &db ]( auto const& e ){ auto t = titles::titles{}; 
                                                        db.execute( insert_into( t ).set( t.uuid = to_string( e ), t.title = to_string( e ) ) ); } );
        repair_missing_entry( db
                            , "body"
                            , [ &db ]( auto const& e ) -> bool { return db.fetch_body( e ).has_value(); }
                            , [ &db ]( auto const& e ){ auto t = bodies::bodies{}; 
                                                        db.execute( insert_into( t ).set( t.uuid = to_string( e ), t.body = "" ) ); } );
        // repair_missing_entry( db
        //                     , "genesis_time"
        //                     , [ &db ]( auto const& e ) -> bool { return db.fetch_genesis_time( e ).has_value(); }
        //                     , [ &db ]( auto const& e ){ auto t = genesis_times::genesis_times{}; 
        //                                                 db.execute( insert_into( t ).set( t.uuid = to_string( e ), t.unix_time = present_time() ) ); } );
        fmt::print( "Analyzing heading conflicts...\n" );
        repair_conflicting_headings( db ); // depends_on( "repair_missing_nodes" )

        rv = outcome::success();
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::filesystem::file_not_found );
    }

    return rv;
}

namespace {

namespace check_state_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const fs_path = args.get( 0 );
const res = kmap.check_state( fs_path );

if( res.has_value() )
{
    kmap.select_node( kmap.selected_node() );

    rv = kmap.success( 'check complete' );
}
else
{
    rv = res.error_message();
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "examines state of the target map for abnormalities";
auto const arguments = std::vector< Argument >{ Argument{ "file_path"
                                                        , "path of map to be examined"
                                                        , "filesystem_path" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    check.state
,   description 
,   arguments
,   guard
,   action
);

} // namespace check_state_def

namespace repair_state_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const fs_path = args.get( 0 );
const res = kmap.repair_state( fs_path );

if( res.has_value() )
{
    kmap.select_node( kmap.selected_node() );

    rv = kmap.success( 'repaired' );
}
else
{
    rv = res.error_message();
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "attempts to repair state of the target map";
auto const arguments = std::vector< Argument >{ Argument{ "file_path"
                                                        , "path of map to be repaired"
                                                        , "filesystem_path" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    repair.state
,   description 
,   arguments
,   guard
,   action
);

} // namespace repair_state_def

namespace binding {

using namespace emscripten;

auto check_state( std::string const& fs_path )
    -> kmap::binding::Result< void >
{
    return repair::check_state( fs_path );
}

auto repair_state( std::string const& fs_path )
    -> kmap::binding::Result< void >
{
    return repair::repair_state( fs_path );
}

EMSCRIPTEN_BINDINGS( kmap_module )
{
    function( "check_state", &kmap::cmd::repair::binding::check_state );
    function( "repair_state", &kmap::cmd::repair::binding::repair_state );
}

} // namespace binding

} // namespace anon

} // namespace kmap::cmd::repair