/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <cmd/repair.hpp>

#include <cmd/command.hpp>
#include <com/cmd/command.hpp>
#include <com/database/db.hpp>
#include <com/database/db.hpp>
#include <com/database/table_decl.hpp>
#include <com/database/util.hpp>
#include <com/filesystem/filesystem.hpp>
#include <com/task/task.hpp>
#include <common.hpp>
#include <contract.hpp>
#include <error/filesystem.hpp>
#include <error/master.hpp>
#include <io.hpp>
#include <kmap.hpp>
#include <kmap/binding/js/result.hpp>
#include <test/util.hpp>
#include <utility.hpp>

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/sqlite3/insert_or.h>
#include <sqlpp11/sqlpp11.h>

#include <map>

namespace fs = boost::filesystem;
namespace rvs = ranges::views;
namespace sql = sqlpp::sqlite3;
namespace db = kmap::com::db;
using sqlpp::sqlite3::insert_or_replace_into;

namespace kmap::repair {

namespace {

template< typename CheckFn
        , typename RepairFn >
auto repair_missing_entry( com::Database& db
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

auto back_up_state( FsPath const& fp )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", fp.string() );

    auto rv = result::make_result< void >();
    auto const bkup_fp = FsPath{ fp }.replace_extension( ".pre-repair" );
    auto ec = boost::system::error_code{};

    fmt::print( "Backing up before performing repairs: {}...\n"
                , bkup_fp.string() );
    
    fs::copy_file( fp
                 , bkup_fp
                 , fs::copy_options::overwrite_existing
                 , ec );
    
    if( ec.value() != 0 )
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, ec.message() );
    }
    else
    {
        rv = outcome::success();
    }

    return rv;
}

auto check_for_tables( sql::connection& con
                     , bool fix )
    -> Result< void >
{
    auto rv = result::make_result< void >();
    auto alls_well = true;
    auto const expected_tables = [ & ]
    {
        auto const r = com::db::table_map
                     | rvs::keys;
        return r | ranges::to< std::vector< std::string > >();
    }();
    auto const queried_tables = [ & ]
    {
        auto const m = com::execute_raw( con, "SELECT * FROM sqlite_master WHERE type='table';" );
        auto rs = std::set< std::string >();
        for( auto er = m.equal_range( std::string{ "tbl_name" } )
            ; er.first != er.second
            ; ++er.first )
        {
            rs.emplace( er.first->second );
        }
        return rs | ranges::to< std::vector< std::string > >();
    }();
    auto const deltas = rvs::set_difference( expected_tables, queried_tables )
                     | ranges::to< std::set< std::string > >();

    for( auto const& delta : deltas )
    {
        io::print( "[log][error] Abnormality detected: missing ('{}') table\n"
                 , delta );
        
        if( fix )
        {
            io::print( "[log][repair] Creating missing table: '{}'\n"
                     , delta );
            
            con.execute( com::db::table_map.at( delta ) );
        }
        else
        {
            alls_well = false;
        }
    }

    // TODO: In theory, this should check that the table is well formed, as well.
    //       Can be done via `PRAGMA table_info(<table>);`
    //       Would probably require altering db::table_map to a struct containing a set of k,v that can be compared with the result of PRAGMA table_info(<table>).

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto check_ids_for_valid_format( sql::connection& con
                               , bool fix )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    bool alls_well = true;
    auto const check_id = []( auto const& id )
    {
        if( uuid_from_string( id ).has_error() )
        {
            io::print( "[log][error] Abnormality detected: ({}) invalid ID detected\n"
                     , std::string{ id } );

            return false;
        }
        else
        {
            return true;
        }
    };

    auto check = [ & ]( auto table
                      , auto const& table_name
                      , auto fix_set
                      , auto fix_set_insert
                      , auto const& validate 
                      , auto remove_entry )
    {
        io::print( "[log] Checking '{}' for invalid IDs\n", table_name );
        for( auto rows = con( select( all_of( table ) )
                            . from( table )
                            . unconditionally() )
           ; auto const& e : rows )
        {
            if( !validate( e ) )
            {
                if( fix )
                {
                    fix_set_insert( fix_set, e );
                }
                else
                {
                    alls_well = false;
                }
            }
        }

        for( auto const& e : fix_set )
        {
            io::print( "[log][repair] Erasing invalid ID entry...\n" );

            remove_entry( table, e );
        }
    };

    check( headings::headings{}
         , "headings"
         , std::set< std::string >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( e.uuid ); }
         , [ & ]( auto const& e ){ return check_id( e.uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.uuid == e ) ); } );
    check( titles::titles{}
         , "titles"
         , std::set< std::string >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( e.uuid ); }
         , [ & ]( auto const& e ){ return check_id( e.uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.uuid == e ) ); } );
    check( bodies::bodies{}
         , "bodies"
         , std::set< std::string >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( e.uuid ); }
         , [ & ]( auto const& e ){ return check_id( e.uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.uuid == e ) ); } );
    check( children::children{}
         , "children"
         , std::set< std::pair< std::string, std::string > >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( std::pair{ e.parent_uuid, e.child_uuid } ); }
         , [ & ]( auto const& e ){ return check_id( e.parent_uuid ) && check_id( e.child_uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.parent_uuid == e.first && table.child_uuid == e.second ) ); } );
    check( aliases::aliases{}
         , "aliases"
         , std::set< std::pair< std::string, std::string > >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( std::pair{ e.src_uuid, e.dst_uuid } ); }
         , [ & ]( auto const& e ){ return check_id( e.src_uuid ) && check_id( e.dst_uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.src_uuid == e.first && table.dst_uuid == e.second ) ); } );
    check( attributes::attributes{}
         , "attributes"
         , std::set< std::pair< std::string, std::string > >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( std::pair{ e.parent_uuid, e.child_uuid } ); }
         , [ & ]( auto const& e ){ return check_id( e.parent_uuid ) && check_id( e.child_uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.parent_uuid == e.first && table.child_uuid == e.second ) ); } );
    check( resources::resources{}
         , "resources"
         , std::set< std::string >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( e.uuid ); }
         , [ & ]( auto const& e ){ return check_id( e.uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.uuid == e ) ); } );
    check( nodes::nodes{}
         , "nodes"
         , std::set< std::string >{}
         , [ & ]( auto& fs, auto const& e ){ return fs.emplace( e.uuid ); }
         , [ & ]( auto const& e ){ return check_id( e.uuid ); }
         , [ & ]( auto table, auto e ) mutable { con( remove_from( table ).where( table.uuid == e ) ); } );

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

/// Checks all tables for node table mapping. If not found (and fix), erase node.
auto check_all_against_node_table( sql::connection& con
                                 , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    bool alls_well = true;
    auto repair_set = std::set< std::string >{};

    // Check that each table record maps to an entry in the "nodes" table.
    // check_nodes_table_mappings( "headings" );
    // check_node_table_mappings( "titles" );
    auto const all_nodes = [ & ]
    {
        auto ns = nodes::nodes{};
        auto rv = std::set< std::string >{};
        for( auto rows = con( select( all_of( ns ) )
                            . from( ns )
                            . unconditionally() )
           ; auto const& e : rows )
        {
            rv.emplace( std::string{ e.uuid } );
        }
        return rv;
    }();
    auto check_single = [ & ]( auto const tbl
                             , auto const tbl_name )
    {
        for( auto rows = con( select( all_of( tbl ) ).from( tbl ).unconditionally() )
           ; auto const& e : rows )
        {
            if( !( all_nodes.contains( std::string{ e.uuid } ) ) )
            {
                io::print( "[log][error] Abnormality detected: ({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.uuid } );

                if( fix )
                {
                    repair_set.emplace( e.uuid );
                }
                else
                {
                    alls_well = false;
                }
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

        for( auto rows = con( select( all_of( tbl ) ).from( tbl ).unconditionally() )
           ; auto const& e : rows )
        {
            if( !( all_nodes.contains( std::string{ e.parent_uuid } ) ) )
            {
                io::print( "[log][error] Abnormality detected: child:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.parent_uuid } );

                if( fix )
                {
                    repair_set.emplace( e.parent_uuid );
                }
                else
                {
                    alls_well = false;
                }
            }
            if( !( all_nodes.contains( std::string{ e.child_uuid } ) ) )
            {
                io::print( "[log][error] Abnormality detected: child:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.child_uuid } );

                if( fix )
                {
                    repair_set.emplace( e.child_uuid );
                }
                else
                {
                    alls_well = false;
                }
            }
        }
    }
    {
        io::print( "[log] checking: aliases\n" );

        auto tbl = aliases::aliases{};

        for( auto rows = con( select( all_of( tbl ) ).from( tbl ).unconditionally() )
           ; auto const& e : rows )
        {
            if( !( all_nodes.contains( std::string{ e.src_uuid } ) ) )
            {
                io::print( "[log][error] Abnormality detected: alias src:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.src_uuid } );

                if( fix )
                {
                    repair_set.emplace( e.src_uuid );
                }
                else
                {
                    alls_well = false;
                }
            }
            if( !( all_nodes.contains( std::string{ e.dst_uuid } ) ) )
            {
                io::print( "[log][error] Abnormality detected: alias dst:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.dst_uuid } );

                if( fix )
                {
                    repair_set.emplace( e.dst_uuid );
                }
                else
                {
                    alls_well = false;
                }
            }
        }
    }
    // attr
    {
        io::print( "[log] checking: attributes\n" );

        auto tbl = attributes::attributes{};

        for( auto rows = con( select( all_of( tbl ) ).from( tbl ).unconditionally() )
           ; auto const& e : rows )
        {
            if( !( all_nodes.contains( std::string{ e.parent_uuid } ) ) )
            {
                io::print( "[log][error] Abnormality detected: attribute parent:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.parent_uuid } );

                if( fix )
                {
                    repair_set.emplace( e.parent_uuid );
                }
                else
                {
                    alls_well = false;
                }
            }
            if( !( all_nodes.contains( std::string{ e.child_uuid } ) ) )
            {
                io::print( "[log][error] Abnormality detected: attribute child:({}) does not correspond to any entry in nodes table\n"
                         , std::string{ e.child_uuid } );

                if( fix )
                {
                    repair_set.emplace( e.child_uuid );
                }
                else
                {
                    alls_well = false;
                }
            }
        }
    }

    for( auto const& e : repair_set )
    {
        io::print( "[log][repair] Erasing entry for tables: {}\n"
                , e );

        KTRY( erase_node( con, e ) );
    }

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto fetch_unbegotten_nodes( sql::connection& con )
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< UuidSet >();

    auto all_nodes = UuidSet{};
    auto all_children = UuidSet{};
    auto all_attrs = UuidSet{};
    {
        auto table = nodes::nodes{};
        for( auto rows = con( select( all_of( table ) )
                            . from( table )
                            . unconditionally() )
            ; auto const& e : rows )
        {
            all_nodes.emplace( KTRY( uuid_from_string( std::string{ e.uuid } ) ) );
        }
    }
    {
        auto table = children::children{};
        for( auto rows = con( select( all_of( table ) )
                            . from( table )
                            . unconditionally() )
            ; auto const& e : rows )
        {
            all_children.emplace( KTRY( uuid_from_string( std::string{ e.child_uuid } ) ) );
        }
    }
    {
        auto table = attributes::attributes{};
        for( auto rows = con( select( all_of( table ) )
                            . from( table )
                            . unconditionally() )
            ; auto const& e : rows )
        {
            all_attrs.emplace( KTRY( uuid_from_string( std::string{ e.child_uuid } ) ) );
        }
    }

    rv = all_nodes
       | rvs::filter( [ & ]( auto const& e ){ return !all_children.contains( e ) && !all_attrs.contains( e ); } )
       | ranges::to< UuidSet >();

    return rv;
}

// TODO: This needs to be an internal (to module) node, as it makes certain assumptions, such as that it leaves the map in a corrupt state child.
auto erase_node( sql::connection& con
               , std::string const& node )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    io::print( "[log][repair] Erasing node: '{}'\n", node );

    auto rv = result::make_result< void >();
    auto bt = bodies::bodies{};
    auto ht = headings::headings{};
    auto rt = resources::resources{};
    auto tt = titles::titles{};
    auto at = aliases::aliases{};
    auto att = attributes::attributes{};
    auto ct = children::children{};
    auto nt = nodes::nodes{};

    // Erase attributes - These are basically act as "children", so need to be erased like children.
    {
        auto attrs = [ & ]
        {
            auto rows = con( select( all_of( att ) ).from( att ).where( att.parent_uuid == node ) );
            auto rs = std::set< std::string >{};
            for( auto const& row : rows )
            {
                rs.emplace( row.child_uuid );
            }
            return rs;
        }();

        for( auto const& attr : attrs )
        {
            KTRY( erase_node( con, attr ) );
        }
    }
    // Erase children.
    {
        auto children = [ & ]
        {
            auto rows = con( select( all_of( ct ) ).from( ct ).where( ct.parent_uuid == node ) );
            auto rs = std::set< std::string >{};
            for( auto const& row : rows )
            {
                rs.emplace( row.child_uuid );
            }
            return rs;
        }();

        for( auto const& child : children )
        {
            KTRY( erase_node( con, child ) );
        }
    }

    // Erase node from all tables.
    // We are erasing leaves here. So that needs to be kept in mind when considering which table entries to erase.
    con( remove_from( bt ).where( bt.uuid == node ) );
    con( remove_from( ht ).where( ht.uuid == node ) );
    con( remove_from( rt ).where( rt.uuid == node ) );
    con( remove_from( tt ).where( tt.uuid == node ) );
    con( remove_from( at ).where( at.src_uuid == node ) );
    con( remove_from( at ).where( at.dst_uuid == node ) );
    con( remove_from( ct ).where( ct.child_uuid == node ) );
    con( remove_from( ct ).where( ct.parent_uuid == node ) );
    con( remove_from( att ).where( att.child_uuid == node ) );
    con( remove_from( att ).where( att.parent_uuid == node ) );
    con( remove_from( nt ).where( nt.uuid == node ) );

    // Note: What about relation info, such as <parent>.$.order? I think this can all be accounted for when traversal is done.
    // Such as to leave the map a corrupted state which will be automatically fixed later.

    rv = outcome::success();

    return rv;
}

auto check_orphaned_nodes( sql::connection& con
                         , Uuid const& root
                         , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto alls_well = true;

    auto const unbegottens = KTRY( fetch_unbegotten_nodes( con ) );

    KMAP_ENSURE_MSG( unbegottens.contains( root ), error_code::common::uncategorized, "provided 'root' not a true root node" );

    if( unbegottens.size() > 1 )
    {
        for( auto const& ubn : unbegottens )
        {
            io::print( "[log][error] Abnormality detected: non-root unbegotten (parentless) node detected ('{}')\n"
                     , to_string( ubn ) );
        }

        if( fix )
        {
            for( auto const& ubn : unbegottens
                                 | rvs::filter( [ &root ]( auto const& e ){ return e != root; } ) )
            {
                KTRY( repair::erase_node( con, to_string( ubn ) ) );
            }
        }
        else
        {
            alls_well = false;
        }
    }

    // Double-check to ensure `root` is only unbegotten node.
    {
        auto const ubns = KTRY( fetch_unbegotten_nodes( con ) );
        KMAP_ENSURE( ubns.size() == 1 && ubns.contains( root ), error_code::common::uncategorized );
    }

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

// A node should have at most one parent node (attributes inclusive).
auto check_multiparent( sql::connection& con
                      , std::string const& node
                      , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto alls_well = true;
    auto ct = children::children{};
    auto att = attributes::attributes{};

    // TODO: This should combine children and attributes, as a node can't legally be in both.
    // children
    {
        auto rows = con( select( all_of( ct ) ).from( ct ).where( ct.child_uuid == node ) );

        if( std::distance( rows.begin(), rows.end() ) > 1 )
        {
            io::print( "[log][error] Abnormality detected: multiple parents detected for child: '{}'. Unable to proceed; aborting.\n"
                     , node );
            
            alls_well = false;
        }
    }
    // attributes
    {
        auto rows = con( select( all_of( att ) ).from( att ).where( att.child_uuid == node ) );

        if( std::distance( rows.begin(), rows.end() ) > 1 )
        {
            io::print( "[log][error] Abnormality detected: multiple parents detected for attribute: '{}'\n"
                     , node );

            alls_well = false;
        }
    }

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto check_heading( sql::connection& con
                  , std::string const& node
                  , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto ht = headings::headings{};
    auto alls_well = true;
    
    // Heading exists
    {
        auto rows = con( select( all_of( ht ) ).from( ht ).where( ht.uuid == node ) );

        if( std::distance( rows.begin(), rows.end() ) != 1 )
        {
            io::print( "[log][error] Abnormality detected: did not find exactly one heading for node: '{}'\n"
                     , node );

            alls_well = false;
        }
        else
        {
            // Heading had valid format
            auto const heading = KTRYE( com::db::select_heading( con, node ) );

            if( !is_valid_heading( heading ) )
            {
                io::print( "[log][error] Abnormality detected: did not find exactly one heading for node: '{}'\n"
                        , node );

                if( fix )
                {
                    auto const fixed_heading = format_heading( heading );

                    fmt::print( "[log][repair] Repairing conflicting child heading for: [ {}, {} ]\n"
                                , heading 
                                , fixed_heading );
                    
                    con( insert_or_replace_into( ht ).set( ht.uuid = node, ht.heading = fixed_heading ) );
                }
                else
                {
                    alls_well = false;
                }
            }
        }
    }

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto check_heading_conflict( sql::connection& con
                           , std::string const& node
                           , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto ht = headings::headings{};
    auto alls_well = true;
    
    // Heading conflicts
    {
        auto headings = [ & ]
        {
            auto rmap = std::multimap< std::string, std::string >{};

            if( auto const parent = com::db::select_parent( con, node )
              ; parent )
            {
                for( auto const siblings = KTRYE( com::db::select_children( con, parent.value() ) )
                   ; auto const& sibling : siblings )
                {
                    for( auto rows = con( select( all_of( ht ) ).from( ht ).where( ht.uuid == sibling ) )
                       ; auto const& row : rows )
                    {
                        rmap.emplace( row.heading, row.uuid );
                    }
                }
            }
            else if( auto const parent = com::db::select_attr_parent( con, node )
                   ; parent )
            {
                for( auto const siblings = KTRYE( com::db::select_attributes( con, parent.value() ) )
                   ; auto const& sibling : siblings )
                {
                    for( auto rows = con( select( all_of( ht ) ).from( ht ).where( ht.uuid == sibling ) )
                       ; auto const& row : rows )
                    {
                        rmap.emplace( row.heading, row.uuid );
                    }
                }
            }
            else
            {
                KMAP_THROW_EXCEPTION_MSG( fmt::format( "Expected parent or attribute parent to node: '{}'\n", node ) );
            }

            return rmap;
        }();
        auto const node_heading = KTRYE( com::db::select_heading( con, node ) );
        auto const& [ rb, re ] = headings.equal_range( node_heading );

        if( std::distance( rb, re ) != 1 )
        {
            io::print( "[log][error] Abnormality detected: conflicting heading found for: '{}'\n"
                        , node );

            if( fix )
            {
                for( auto it = rb
                   ; it != re
                   ; ++it )
                {
                    io::print( "[log][repair] Repairing conflicting node heading for: {}\n"
                             , it->first );

                    auto const dist = std::distance( rb, it );
                    auto const new_heading = fmt::format( "{}_conflict_{}"
                                                        , node_heading
                                                        , dist );

                    con( insert_or_replace_into( ht ).set( ht.uuid = it->first, ht.heading = new_heading ) );
                }
            }
            else
            {
                alls_well = false;
            }
        }
    }

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

// TODO: This should erase bodies if they're found to be empty.
auto check_body( sql::connection& con
                , std::string const& node
                , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto alls_well = true;
    auto bt = bodies::bodies{};
    
    {
        // No specific formatting for title, so just ensure that the node has a title.
        auto rows = con( select( all_of( bt ) ).from( bt ).where( bt.uuid == node ) );

        if( std::distance( rows.begin(), rows.end() ) > 1 )
        {
            io::print( "[log][error] Abnormality detected: did found multiple bodies for node: '{}'\n"
                     , node );

            alls_well = false;
        }
    }

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto check_title( sql::connection& con
                , std::string const& node
                , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto alls_well = true;
    auto tt = titles::titles{};
    
    {
        auto rows = con( select( all_of( tt ) ).from( tt ).where( tt.uuid == node ) );

        if( auto const count = std::distance( rows.begin(), rows.end() )
          ; count != 1 )
        {
            io::print( "[log][error] Abnormality detected: did not find exactly one title for node: '{}' (found: {})\n"
                     , node
                     , count );

            alls_well = false;
        }
    }

    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto check_has_no_attr( sql::connection& con
                      , std::string const& node
                      , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto alls_well = true;

    if( auto const attrs = com::db::select_attributes( con, node )
      ; attrs && attrs.value().size() > 0 )
    {
        io::print( "[log][error] Abnormality detected: attribute node has attribute for node: '{}'\n"
                 , node );

        if( fix )
        {
            io::print( "[log][repair] Erasing attributes attribute for: '{}'\n"
                     , node );

            for( auto const& attr : attrs.value() )
            {
                KTRY( repair::erase_node( con, attr ) );
            }
        }
        else
        {
            alls_well = false;
        }
    }
    
    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto push_attr( sql::connection& con
              , std::string const& node
              , std::string const& attrn )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto ct = children::children{};

    if( auto const attroot = com::db::select_attribute_root( con, node )
      ; attroot )
    {
        con( insert_into( ct ).set( ct.parent_uuid = attroot.value(), ct.child_uuid = attrn ) );
    }
    else
    {
        auto att = attributes::attributes{};
        auto ht = headings::headings{};
        auto tt = titles::titles{};
        auto const nattroot = to_string( gen_uuid() );

        con( insert_into( att ).set( att.parent_uuid = node, att.child_uuid = nattroot ) );
        con( insert_into( ht ).set( ht.uuid = nattroot, ht.heading = "$" ) );
        con( insert_into( tt ).set( tt.uuid = nattroot, tt.title = "$" ) );
        con( insert_into( ct ).set( ct.parent_uuid = nattroot, ct.child_uuid = attrn ) );
    }

    rv = outcome::success();

    return rv;
}

// TODO: I believe this is the last remaining thing to impl. for repair.
auto check_genesis( sql::connection& con
                  , std::string const& node
                  , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto nt = nodes::nodes{};
    auto ht = headings::headings{};
    auto tt = titles::titles{};
    auto bt = bodies::bodies{};
    auto alls_well = true;
    auto const attrs = KTRYE( com::db::select_attributes( con, node ) );
    auto const attr_headings = attrs
                             | rvs::transform( [ & ]( auto const& e ){ return std::pair{ KTRYE( com::db::select_heading( con, e ) ), e }; } )
                             | ranges::to< std::map< std::string, std::string > >();
    auto const children = KTRYE( com::db::select_children( con, node ) );

    // $.genesis
    if( !attr_headings.contains( "genesis" ) )
    {
        io::print( "[log][error] Abnormality detected: no genesis attribute found for node: '{}'\n"
                 , node );

        if( fix )
        {
            io::print( "[log][repair] Generating genesis attribute, with current timestamp, for node : '{}'\n"
                     , node );

            auto const genesisn = to_string( gen_uuid() );

            con( insert_into( nt ).set( nt.uuid = genesisn ) );
            con( insert_into( ht ).set( ht.uuid = genesisn, ht.heading = "genesis" ) );
            con( insert_into( tt ).set( tt.uuid = genesisn, tt.title = "Genesis" ) );
            con( insert_into( bt ).set( bt.uuid = genesisn, bt.body = std::to_string( present_time() ) ) );

            KTRY( push_attr( con, node, genesisn ) );
        }
        else
        {
            alls_well = false;
        }
    }
    
    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto check_order( sql::connection& con
                , std::string const& node
                , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto nt = nodes::nodes{};
    auto ht = headings::headings{};
    auto bt = bodies::bodies{};
    auto tt = titles::titles{};
    auto alls_well = true;
    auto const attrs = KTRYE( com::db::select_attributes( con, node ) );
    auto const attr_headings = attrs
                             | rvs::transform( [ & ]( auto const& e ){ return std::pair{ KTRYE( com::db::select_heading( con, e ) ), e }; } )
                             | ranges::to< std::map< std::string, std::string > >();
    auto const children = [ & ]
    {
        auto as = KTRYE( com::db::select_aliases( con, node ) );
        auto cs = KTRYE( com::db::select_children( con, node ) );
        cs.insert( as.begin(), as.end() );
        return cs;
    }();

    if( !children.empty() && !attr_headings.contains( "order" ) )
    {
        io::print( "[log][error] Abnormality detected: NO 'order' attribute found for node with children: '{}'\n"
                 , node );

        if( fix )
        {
            io::print( "[log][repair] Generating 'order' attribute for: '{}'\n"
                     , node );

            // TODO: Should probably order by genesis node, but that requires that this is called after children are processed, to ensure that they have genesis nodes.
            //       But.. interestingly, an alias doesn't have a genesis time recorded, only its source genesis.
            auto const body = children | rvs::join( '\n' ) | ranges::to< std::string >(); //[ & ]
            auto const ordern = to_string( gen_uuid() );

            con( insert_into( nt ).set( nt.uuid = ordern ) );
            con( insert_into( ht ).set( ht.uuid = ordern, ht.heading = "order" ) );
            con( insert_into( tt ).set( tt.uuid = ordern, tt.title = "Order" ) );
            con( insert_into( bt ).set( bt.uuid = ordern, bt.body = body ) );

            KTRY( push_attr( con, node, ordern ) );
        }
        else
        {
            alls_well = false;
        }
    }
    else if( children.empty() && attr_headings.contains( "order" ) )
    {
        io::print( "[log][error] Abnormality detected: 'order' attribute found for node with NO children: '{}'\n"
                 , node );

        if( fix )
        {
            io::print( "[log][repair] Erasing 'order' attribute for: '{}'\n"
                     , node );

            KTRY( erase_node( con, attr_headings.at( "order" ) ) );
        }
        else
        {
            alls_well = false;
        }
    }
    else if( !children.empty() && attr_headings.contains( "order" ) )
    {
        auto const order_set = [ & ]
        {
            auto rs = std::set< std::string >{};
            auto const aliases = KTRYE( com::db::select_aliases( con, node ) );

            rs.insert( children.begin(), children.end() );
            rs.insert( aliases.begin(), aliases.end() );

            return rs;
        }();
        auto const body = KTRYE( com::db::select_body( con, attr_headings.at( "order" ) ) );
        auto const body_set = body
                            | rvs::split( '\n' )
                            | ranges::to< std::set< std::string > >();
        auto order_vec = order_set | ranges::to< std::vector< std::string > >();
        auto body_vec = body_set | ranges::to< std::vector< std::string > >();

        ranges::sort( order_vec );
        ranges::sort( body_vec );

        for( auto const& d : rvs::set_difference( order_vec, body_vec ) )
        {
            // Existent child not represented in body; insert
            io::print( "[log][error] Abnormality detected: 'order' attribute missing node entry: '{}', for node: '{}'\n"
                     , d
                     , node );

            if( fix )
            {
                io::print( "[log][repair] Inserting missing node entry for 'order': '{}'\n"
                         , d );

                auto const new_body = [ & ]
                {
                    if( auto const tbody = KTRYE( com::db::select_body( con, attr_headings.at( "order" ) ) )
                      ; tbody.empty() )
                    {
                        return d;
                    }
                    else
                    {
                        return fmt::format( "{}\n{}", tbody, d );
                    }
                }();

                con( insert_or_replace_into( bt ).set( bt.uuid = attr_headings.at( "order" ), bt.body = new_body ) );
            }
            else
            {
                alls_well = false;
            }
        }
        for( auto const& d : rvs::set_difference( body_vec, order_vec ) )
        {
            // Non-existent child represented in body; remove
            io::print( "[log][error] Abnormality detected: 'order' attribute contains non-existent node entry: '{}'\n"
                     , d );

            if( fix )
            {
                auto const new_body = body_vec
                                    | rvs::remove( d )
                                    | rvs::join( '\n' )
                                    | ranges::to< std::string >();

                con( insert_or_replace_into( bt ).set( bt.uuid = attr_headings.at( "order" ), bt.body = new_body ) );
            }
            else
            {
                alls_well = false;
            }
        }
    }
    
    if( alls_well )
    {
        rv = outcome::success();
    }

    return rv;
}

auto traverse_check_attr_node( sql::connection& con
                             , std::string const& node
                             , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( check_multiparent( con, node, fix ) );
    KTRY( check_heading( con, node, fix ) );
    KTRY( check_heading_conflict( con, node, fix ) );
    KTRY( check_body( con, node, fix ) );
    KTRY( check_title( con, node, fix ) );
    KTRY( check_has_no_attr( con, node, fix ) );

    for( auto const children = KTRYE( com::db::select_children( con, node ) )
       ; auto const& child : children )
    {
        KTRY( traverse_check_attr_node( con, child, fix ) );
    }

    rv = outcome::success();

    return rv;
}

auto traverse_check_node( sql::connection& con
                        , std::string const& node
                        , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( check_multiparent( con, node, fix ) );
    KTRY( check_heading( con, node, fix ) );
    KTRY( check_heading_conflict( con, node, fix ) );
    KTRY( check_body( con, node, fix ) );
    KTRY( check_title( con, node, fix ) );
    KTRY( check_genesis( con, node, fix ) );
    KTRY( check_order( con, node, fix ) );

    for( auto const children = KTRYE( com::db::select_children( con, node ) )
       ; auto const& child : children )
    {
        KTRY( traverse_check_node( con, child, fix ) );
    }
    for( auto const attrs = KTRYE( com::db::select_attributes( con, node ) )
       ; auto const& attr : attrs )
    {
        KTRY( traverse_check_attr_node( con, attr, fix ) );
    }

    rv = outcome::success();

    return rv;
}

auto traverse_check_root( sql::connection& con
                        , std::string const& node
                        , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( check_heading( con, node, fix ) );
    KTRY( check_body( con, node, fix ) );
    KTRY( check_title( con, node, fix ) );
    KTRY( check_genesis( con, node, fix ) );
    KTRY( check_order( con, node, fix ) );

    for( auto const children = KTRYE( com::db::select_children( con, node ) )
       ; auto const& child : children )
    {
        KTRY( traverse_check_node( con, child, fix ) );
    }
    for( auto const attrs = KTRYE( com::db::select_attributes( con, node ) )
       ; auto const& attr : attrs )
    {
        KTRY( traverse_check_attr_node( con, attr, fix ) );
    }

    rv = outcome::success();

    return rv;
}

auto traverse_check_root( sql::connection& con
                        , Uuid const& node
                        , bool fix )
    -> Result< void >
{
    return traverse_check_root( con, to_string( node ), fix );
}

// TODO: Should be renamed check_map, as the fix is a toggle switch.
auto check_map( sql::connection& con
              , Uuid const& root
              , bool fix )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "root", to_string( root ) );

    auto rv = result::make_result< void >();

    KTRY( check_for_tables( con, fix ) );
    KTRY( check_ids_for_valid_format( con, fix ) );
    KTRY( check_orphaned_nodes( con, root, fix ) );
    KTRY( check_all_against_node_table( con, fix ) );
    KTRY( traverse_check_root( con, root, fix ) );

    rv = outcome::success();

    return rv;
}

namespace {
namespace binding {

using namespace emscripten;

auto check_map( std::string const& fs_path )
    -> kmap::Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", fs_path );

    auto const fp = com::kmap_root_dir / fs_path;
    
    KMAP_ENSURE( fs::exists( fp ), error_code::filesystem::file_not_found );

    auto con = db::open_connection( fp, SQLITE_OPEN_READWRITE, false );
    auto const unbegotten_set = KTRY( fetch_unbegotten_nodes( con ) );

    if( unbegotten_set.size() == 0 )
    {
        return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "no root found, I frankly don't know how this is possible; congradulations" );
    }
    else if( unbegotten_set.size() > 1 )
    {
        return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "multiple roots found! True root must be supplied via repair_map_with_root" );
    }
    else
    {
        return repair::check_map( con
                                , *unbegotten_set.begin()
                                , false );
    }
}

auto check_map_root( std::string const& fs_path 
                   , std::string const& root_id )
    -> kmap::Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", fs_path );

    auto const fp = com::kmap_root_dir / fs_path;
    
    KMAP_ENSURE( fs::exists( fp ), error_code::filesystem::file_not_found );

    auto con = db::open_connection( fp, SQLITE_OPEN_READWRITE, false );

    return repair::check_map( con
                            , KTRY( uuid_from_string( root_id ) )
                            , false );
}

auto erase_node( std::string const& fs_path 
               , std::string const& node_id )
    -> kmap::Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", fs_path );

    auto const fp = com::kmap_root_dir / fs_path;
    
    KMAP_ENSURE( fs::exists( fp ), error_code::filesystem::file_not_found );

    auto con = db::open_connection( fp, SQLITE_OPEN_READWRITE, false );

    return repair::erase_node( con, node_id );
}

auto repair_map( std::string const& fs_path )
    -> kmap::Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", fs_path );

    auto const fp = com::kmap_root_dir / fs_path;

    KMAP_ENSURE( fs::exists( fp ), error_code::filesystem::file_not_found );

    KTRY( back_up_state( fp ) );

    auto con = db::open_connection( fp, SQLITE_OPEN_READWRITE, false );
    auto const unbegotten_set = KTRY( fetch_unbegotten_nodes( con ) );

    if( unbegotten_set.size() == 0 )
    {
        return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "no root found, I frankly don't know how this is possible; congradulations" );
    }
    else if( unbegotten_set.size() > 1 )
    {
        return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "multiple roots found! True root must be supplied via repair_map_with_root" );
    }
    else
    {
        return repair::check_map( con
                                , *unbegotten_set.begin()
                                , true );
    }
}

auto repair_map_root( std::string const& fs_path
                    , std::string const& root_id )
    -> kmap::Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", fs_path );

    auto const fp = com::kmap_root_dir / fs_path;

    KMAP_ENSURE( fs::exists( fp ), error_code::filesystem::file_not_found );

    KTRY( back_up_state( fp ) );

    auto con = db::open_connection( fp, SQLITE_OPEN_READWRITE, false );

    return repair::check_map( con
                            , KTRY( uuid_from_string( root_id ) )
                            , true );
}

EMSCRIPTEN_BINDINGS( kmap_module )
{
    function( "check_map", &kmap::repair::binding::check_map );
    function( "check_map_root", &kmap::repair::binding::check_map_root );
    function( "erase_node", &kmap::repair::binding::erase_node );
    function( "repair_map", &kmap::repair::binding::repair_map );
    function( "repair_map_root", &kmap::repair::binding::repair_map_root );
}

} // namespace binding

} // namespace anon

SCENARIO( "repair_map", "[repair]" )
{
    // GIVEN( "./current.dump.kmap" )
    // {
    //     REQUIRE( binding::check_map( "current.dump.kmap") );
    // }
#if 0
    GIVEN( "database, with task and subtask, on disk" )
    {
        KMAP_COMPONENT_FIXTURE_SCOPED( "task_store", "filesystem" );

        auto& km = kmap::Singleton::instance();
        auto const tstore = REQUIRE_TRY( km.fetch_component< com::TaskStore >() );
        auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

        REQUIRE_TRY( tstore->create_subtask( t1, "2" ) );

        // Save to file. This is the "good" state.

        auto const fp = com::kmap_root_dir / fmt::format( "repair.test.{}.kmap", to_string( gen_uuid() ) );
        auto con = db::open_connection( fp, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, false );
        auto nt = nodes::nodes{};
        auto ht = headings::headings{};
        auto tt = titles::titles{};
        auto ct = children::children{};

        com::create_tables( con );

        GIVEN( "root node" )
        {
            auto const root = gen_uuid();

            con( insert_into( nt ).set( nt.uuid = to_string( root ) ) );
            con( insert_into( ht ).set( ht.uuid = to_string( root ), ht.heading = "root" ) );
            con( insert_into( tt ).set( tt.uuid = to_string( root ), tt.title = "Root" ) );

            THEN( "check => map state correct" )
            {
                REQUIRE_TRY( repair_map( con, root, false ) );
            }

#if KMAP_EXCEPTIONS_ENABLED // Required because the uuid_from_string operates via exception on failure.
            GIVEN( "invalid node table id" )
            {
                auto const invalid_id = std::string{ "039-981" };

                con( insert_into( nt ).set( nt.uuid = invalid_id ) );

                THEN( "check => map state correct" )
                {
                    REQUIRE( test::fail( repair_map( con, root, false ) ) );
                }

                WHEN( "repair map" )
                {
                    REQUIRE_TRY( repair_map( con, root, true ) );

                    THEN( "invalid node id erased" )
                    {
                        auto selected = con( select( all_of( nt ) ).from( nt ).where( nt.uuid == invalid_id ) );

                        REQUIRE( std::distance( selected.begin(), selected.end() ) == 0 );
                    }
                }
            }

            GIVEN( "invalid child id" )
            {
                // TODO: Ensure repair on children table works as well. 
            }
#endif // KMAP_EXCEPTIONS_ENABLED

            GIVEN( "child of root where children.child_uuid not in nodes.uuid" )
            {
                auto const child = gen_uuid();

                con( insert_into( ct ).set( ct.parent_uuid = to_string( root )
                                          , ct.child_uuid = to_string( child ) ) );

                THEN( "check( child as root ) => non-root provided as root error" )
                {
                    REQUIRE( test::fail( repair_map( con, child, false ) ) );
                }
                THEN( "check => map state corrupt; child_uuid not in nodes" )
                {
                    REQUIRE( test::fail( repair_map( con, root, false ) ) );
                }
                THEN( "repair => child_uuid added to node table" )
                {
                    REQUIRE_TRY( repair_map( con, root, true ) );

                    auto selected = con( select( all_of( nt ) ).from( nt ).where( nt.uuid == to_string( child ) ) );

                    REQUIRE( std::distance( selected.begin(), selected.end() ) == 1 );
                }
            }
        }

        // Cleanup database file.
        {
            auto ec = boost::system::error_code{};
            fs::remove( fp, ec );
            REQUIRE( ec.value() == 0 );
        }
    }
#endif // 0
}

} // namespace kmap::repair