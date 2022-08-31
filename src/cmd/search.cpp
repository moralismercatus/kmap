/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "search.hpp"

#include "com/alias/alias.hpp"
#include "com/database/db.hpp"
#include "com/network/network.hpp"
#include "common.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "kmap.hpp"

#include <range/v3/action/sort.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/transform.hpp>
#include <sqlpp11/sqlpp11.h>

#include <regex>

using namespace ranges;

namespace kmap::cmd {

template< typename DbResult >
auto search_bodies( std::regex const& rgx
                  , DbResult result )
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    for( auto const& e : result )
    {
        auto const body = std::string{ e.body };
        auto const sid = std::string{ e.uuid };
        auto const id = uuid_from_string( sid ).value();
        auto matches = std::smatch{};

        if( std::regex_search( body
                             , matches
                             , rgx ) )
        {
            rv.emplace_back( id );
        }
    }

    return rv;
}

template< typename DbResult >
auto search_headings( std::regex const& rgx
                    , DbResult result )
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    for( auto const& e : result )
    {
        auto const heading = std::string{ e.heading };
        auto const sid = std::string{ e.uuid };
        auto const id = uuid_from_string( sid ).value();
        auto matches = std::smatch{};

        if( std::regex_search( heading 
                             , matches
                             , rgx ) )
        {
            rv.emplace_back( id );
        }
    }

    return rv;
}

auto process_search_results( Kmap& kmap
                           , std::string const& srgx
                           , std::vector< Uuid > const& matching_nodes )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const nw = KTRY( kmap_.fetch_component< com::Network>() );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto sorted_matches = matching_nodes;
    auto const count_ancestors = [ &kmap ]( auto const& node ){ return view::make( node ) | view::ancestor | view::count( kmap ); };

    sorted_matches |= actions::sort( [ & ]( auto const& lhs
                                          , auto const& rhs ) { return count_ancestors( lhs ) < count_ancestors( rhs ); } );

    // TODO: If search already exists, replace it?
    auto const search_root_path = fmt::format( "{}.{}"
                                             , searches_root
                                             , format_heading( srgx ) );

    if( nw->exists( search_root_path ) )
    {
        return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "search {} already exists", search_root_path ) );
    }

    auto const search_root = kmap.fetch_or_create_leaf( search_root_path );

    BC_ASSERT( search_root );

    {
        // TODO: Add additional relevant search details, e.g., time of search.
        auto const details = fmt::format( "Regex: `{}`<br>"
                                          "Hits: {}<br>"
                                        , srgx
                                        , sorted_matches.size() );
        KTRY( nw->update_body( *search_root,  details ) );
        KTRY( nw->update_title( *search_root, srgx ) );
    }

    for( auto const [ idx, id ] : views::enumerate( sorted_matches ) )
    {
        auto const iid = nw->create_child( *search_root
                                         , fmt::format( "match_{}"
                                                      , idx ) );
        auto const details = fmt::format( "Path: `{}`<br>iPath: `{}`<br>distance from root: {}"
                                        , kmap.absolute_path_flat( id )
                                        , kmap.absolute_ipath_flat( id )
                                        , view::make( id ) | view::ancestor | view::count( kmap ) );

        KTRY( nw->update_body( iid.value(), details ) );
        KTRY( nw->create_alias( id, iid.value() ) );
    }

    KTRY( kmap.select_node( *search_root ) );

    auto const cumulative_match_count = sorted_matches.size();

    rv = fmt::format( "matches found: {}"
                    , cumulative_match_count );
    
    return rv;
}

auto search_bodies( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{   
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() >= 1 );
            })
        ;
 
        KMAP_THROW_EXCEPTION_MSG( "current search is disabled, refactoring needed" );

        auto const db = KTRYE( kmap.fetch_component< com::Database >() );
        auto const srgx = flatten( args, ' ' );
        auto const raw = db->execute_raw( fmt::format( "SELECT * FROM bodies WHERE body REGEXP '{}'"
                                                     , srgx ) );
        auto const nodes = raw
                         | views::transform( [ & ]( auto const& e ){ return uuid_from_string( e.first ).value(); } )
                         | to_vector;

        return process_search_results( kmap
                                     , srgx
                                     , nodes );
    };
}

auto search_bodies_first( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{   
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() >= 1 );
            })
        ;

        KMAP_THROW_EXCEPTION_MSG( "current search is disabled, refactoring needed" );

        auto const db = KTRYE( kmap.fetch_component< com::Database >() );
        auto const srgx = flatten( args, ' ' );
        auto const raw = db->execute_raw( fmt::format( "SELECT * FROM bodies WHERE body REGEXP '{}' LIMIT 1"
                                                     , srgx ) );
        auto const nodes = raw
                         | views::transform( [ & ]( auto const& e ){ return uuid_from_string( e.first ).value(); } )
                         | to_vector;

        return process_search_results( kmap
                                     , srgx
                                     , nodes );
    };
}

// TODO: This can probably be made obsolete by chaining commands. E.g.: `kmap.nodes | select.leaf | search.body: args`
//       If common, a simple command in the graph could be named search.leaf.bodies that calls the chained commands.
// TODO: Create test for rgx=".*" when all leaf node bodies are empty. Got an error trying this.
#if 0
auto search_leaf_bodies( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{   
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const leaves = kmap.descendant_leaves( kmap.root_node_id() );
        auto const sleaves = leaves
                           | views::transform( [ & ]( auto const& e ){ return to_string( e ); } )
                           | to< std::vector< std::string > >();
        auto const srgx = flatten( args, ' ' );
        auto const matching_nodes = [ & ]
        {
            auto const db = KTRYE( kmap.fetch_component< com::Database >() );
            auto bt = bodies::bodies{};
            using namespace sqlpp;

            return search_bodies( std::regex{ srgx }
                                , db->execute( select( all_of( bt ) )
                                             . from( bt )
                                             . where( bt.uuid.in( sqlpp::value_list( sleaves ) ) ) ) );
        }();

        return process_search_results( kmap
                                     , srgx
                                     , matching_nodes );
    };
}
#endif // 0

auto search_headings( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{   
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() >= 1 );
            })
        ;

        auto const srgx = flatten( args, ' ' );
        auto const matching_nodes = [ & ]
        {
            auto const db = KTRYE( kmap.fetch_component< com::Database >() );
            auto ht = headings::headings{};

            return search_headings( std::regex{ srgx }
                                  , db->execute( select( all_of( ht ) )
                                               . from( ht )
                                               . unconditionally() ) );
        }();

        return process_search_results( kmap
                                     , srgx
                                     , matching_nodes );
    };
}

} // namespace kmap::cmd