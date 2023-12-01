/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/disambiguate.hpp>

#include <attribute.hpp>
#include <com/database/db.hpp>
#include <com/network/network.hpp>
#include <common.hpp>
#include <contract.hpp>
#include <kmap.hpp>
#include <path/act/order.hpp>
#include <test/util.hpp>
#include <util/result.hpp>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/transform.hpp>

#include <map>
#include <string>

namespace rvs = ranges::views;

namespace kmap {

namespace {

auto disambiguate_paths3( Kmap const& km
                        , std::vector< std::pair< Uuid, std::string > > const& lineage
                        , std::string const& heading )
    -> Result< std::map< Uuid, std::string > >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::map< Uuid, std::string > >();
    auto matches = std::vector< Uuid >{};
    auto disambigs = std::map< Uuid, std::string >{};

    for( auto it = lineage.begin(), eit = lineage.end()
       ; it != eit
       ; ++it )
    {
        if( it->second == heading )
        {
            matches.emplace_back( it->first );
        }
    }

    KMAP_ENSURE( matches.size() > 0, error_code::common::uncategorized );

    if( matches.size() == 1 )
    {
        // Don't need to add this, as size == 1 means its already unambiguous.
        // disambigs.emplace( matches[ 0 ], heading );

        disambigs.emplace( matches.front(), "" );

        rv = disambigs;
    }
    else // matches.size() > 1
    {
        for( auto const [ index, id ] : rvs::enumerate( matches ) )
        {
            disambigs.emplace( id, fmt::format( "@[{}]", index ) );
        }

        rv = disambigs;
    }

    return rv;
}

auto disambiguate_paths3( Kmap const& km
                        , std::vector< std::vector< std::pair< Uuid, std::string > > > const& lineages
                        , std::string const& heading )
    -> Result< std::map< Uuid, std::string > >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "heading", heading );

    auto rv = result::make_result< std::map< Uuid, std::string > >();
    auto rmap = std::map< Uuid, std::string >{};

    KMAP_ENSURE( !lineages.empty(), error_code::common::uncategorized );
    KMAP_ENSURE( !lineages[ 0 ].empty(), error_code::common::uncategorized );

    auto common_root = lineages[ 0 ][ 0 ];

    KM_RESULT_PUSH( "common_root", common_root.first );

    #if KMAP_DEBUG || 0
    fmt::print( "new common root\n" );
    #endif // KMAP_DEBUG
    // Ensure all lineages share a common root.
    for( auto const& lineage : lineages )
    {
        KMAP_ENSURE( !lineage.empty(), error_code::common::uncategorized );
        KMAP_ENSURE( lineage[ 0 ] == common_root, error_code::common::uncategorized );

        #if KMAP_DEBUG || 0
        fmt::print( "---disambig lineage: {}\n", KTRY( anchor::node( lineage.front().first ) | view2::right_lineal( lineage.back().first ) | act2::abs_path_flat( km ) ) );
        #endif // KMAP_DEBUG
    }

    if( lineages.size() == 1 )
    {
        rv = KTRY( disambiguate_paths3( km, lineages[ 0 ], heading ) );
    }
    else
    {
        for( auto const& lin : lineages )
        {
            if( lin.size() < 3 )
            {
                rmap.emplace( lin.back().first, "" );
            }
        }
        
        auto const root_dropped = [ & ]
        {
            auto tlins = lineages;

            for( auto&& tlin : tlins )
            {
                tlin.erase( tlin.begin() );
            }

            return tlins
                 | rvs::remove_if( []( auto const& e ){ return e.size() < 2; } )
                 | ranges::to< decltype( tlins ) >();
        }();

        auto const groups = [ & ]
        {
            auto m = std::multimap< Uuid, std::vector< std::pair< Uuid, std::string > > >{};

            for( auto const& lin : root_dropped )
            {
                m.emplace( lin[ 0 ].first, lin );
            }

            auto rg = std::vector< std::vector< std::vector< std::pair< Uuid, std::string > > > >{};

            for( auto const keys = m | rvs::keys | ranges::to< std::set< Uuid > >()
               ; auto const& key : keys )
            {
                auto lin_group = std::vector< std::vector< std::pair< Uuid, std::string > > >{};

                for( auto [ it, eit ] = m.equal_range( key )
                   ; it != eit
                   ; ++it )
                {
                    lin_group.emplace_back( it->second );
                }

                rg.emplace_back( lin_group );
            }

            return rg;
        }();

        for( auto const& group : groups )
        {
            auto const disambigd = KTRY( disambiguate_paths3( km, group, heading ) );
            
            for( auto&& e : disambigd )
            {
                if( groups.size() > 1 ) // Needs disambiguation
                {
                    rmap.emplace( std::pair{ e.first, fmt::format( "@{}{}", group[ 0 ][ 0 ].second, e.second ) } );
                }
                else
                {
                    rmap.emplace( e );
                }
            }
        }

        rv = rmap;
    }

    return rv;
}

} // namespace anon

auto disambiguate_path3( Kmap const& km
                       , std::string const& heading )
    -> Result< std::map< Uuid, std::string > >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::map< Uuid, std::string > >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
        })
    ;

    auto const db = KTRY( km.fetch_component< com::Database >() );
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto ambig_nodes = [ & ]
    {
        auto const nonaliases = db->fetch_nodes( heading );
        auto combined = nonaliases;
        for( auto const& e : nonaliases )
        {
            auto const dsts = nw->alias_store().fetch_aliases( com::AliasItem::rsrc_type{ e } );
            
            combined.insert( dsts.begin(), dsts.end() );
        }
        return combined;
    }();

    // Need to eliminate nodes where duplicate headings exist in same lineage, so correct index can be given to each.
    // Put another way, any ambig node that is an ancestor of another will be accounted for, and needs to be dropped.
    {
        auto const ancestries = [ & ]
        {
            auto lrv = std::set< Uuid >{};

            for( auto const& e : ambig_nodes )
            {
                auto const ancs = anchor::node( e ) | view2::ancestor | act2::to_node_set( km );

                lrv.insert( ancs.begin(), ancs.end() );
            }

            return lrv;
        }();

        // for each node in ambig_nodes, if ancestor( ambig_nodes ), erase it.
        for( auto it = ambig_nodes.begin()
           ; it != ambig_nodes.end()
           ; ++it )
        {
            if( ancestries.contains( *it ) )
            {
                it = ambig_nodes.erase( it );
                break;
            }
        }
    }

    auto const lineages = ambig_nodes
                        | rvs::remove_if( [ & ]( auto const& n ){ return attr::is_in_attr_tree( km, n ); } )
                        | rvs::transform( [ & ]( auto const& n ){ return anchor::node( n )
                                                                       | view2::left_lineal
                                                                       | view2::order
                                                                       | act2::to_node_vec( km ); } )
                        | ranges::to< std::vector< std::vector< Uuid > > >();
    auto const lineages_nhp = lineages
                            | rvs::transform( [ & ]( auto const& v )
                                            { 
                                                return v 
                                                     | rvs::transform( [ & ]( auto const& e )
                                                                       { 
                                                                           return std::pair{ e, KTRYE( nw->fetch_heading( e ) ) };
                                                                       } )
                                                     | ranges::to< std::vector< std::pair< Uuid, std::string > > >(); 
                                            } )
                            | ranges::to< std::vector< std::vector< std::pair< Uuid, std::string > > > >();

    rv = KTRY( disambiguate_paths3( km, lineages_nhp, heading ) );

    return rv;
}

// Maybe... anchor::node( node ) | act2::disambiguate_paths( km ) => results in node:result cache association.
// But actually don't want to limit it to node:result. Better if heading:result association. I don't know if that's possible given the current caching system,
// which uses `Tether` as the "key" to the cache's map; however, there isn't anything keeping me from introducing different caching systems.

auto disambiguate_path3( Kmap const& km
                       , Uuid const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // TODO: Uncomment. Should pass, but doesn't in postcond, it does at end of function. Easily observe by place the assert after rv assigned.
            //       Again, contracts behaving strangely. Worrying. Not the first time. UB manifesting, or EMCC somehow corrupting contracts?
            // if( rv )
            // {
            //     BC_ASSERT( rv.value().contains( node ) );
            // }
        })
    ;

    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto const heading = KTRY( nw->fetch_heading( node ) );
    auto const dpaths =  KTRY( disambiguate_path3( km, heading ) );

    rv = dpaths.at( node );

    return rv;
}

SCENARIO( "disambiguate_path3" , "[path]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = kmap::Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const decide_unique_path = [ & ]( std::string const& path )
    {
        auto const rs = REQUIRE_TRY( decide_path( km, nw->root_node(), nw->selected_node(), path ) );

        REQUIRE( rs.size() == 1 );

        return rs.back();
    };
    auto const check = [ & ]( auto const& iheading, std::map< std::string, std::string > const& opaths ) -> bool
    {
        auto const dises = REQUIRE_TRY( disambiguate_path3( km, iheading ) );

        KENSURE_B( dises.size() == opaths.size() );

        for( auto const& [ id, path ] : dises )
        {
            #if KMAP_DEBUG
            fmt::print( "{} => {}, {}\n", iheading, path, REQUIRE_TRY( absolute_path_flat( km, id ) ) );
            #endif // KMAP_DEBUG

            KENSURE_B( opaths.contains( path ) );

            auto const decided_node = decide_unique_path( opaths.at( path ) );

            KENSURE_B( id == decided_node );
        }

        return true;
    };

    GIVEN( "/a.b.c.e.x\n"
           "/a.b.c.f.x\n"
           "/a.b.c.d.x\n"
           "/a.b.c.d.x.g.d.x\n"
           "/y.g.d.x\n"
           "/x" )
    {
        auto const create = [ & ]( auto const& s ){ REQUIRE_TRY( anchor::abs_root | view2::direct_desc( s ) | act2::create_node( km ) ); };

        create( "a.b.c.e.x" );
        create( "a.b.c.f.x" );
        create( "a.b.c.d.x" );
        create( "a.b.c.d.x.g.d.x" );
        create( "y.g.d.x" );
        create( "x" );

        REQUIRE( check( "a", { { "", "/a" } } ) );
        REQUIRE( check( "b", { { "", "/a.b" } } ) );
        REQUIRE( check( "c", { { "", "/a.b.c" } } ) );
        REQUIRE( check( "d", { { "@a@[0]", "/a.b.c.d" }, { "@a@[1]", "/a.b.c.d.x.g.d" }, { "@y", "/y.g.d" } } ) );
        REQUIRE( check( "e", { { "", "/a.b.c.e" } } ) );
        REQUIRE( check( "f", { { "", "/a.b.c.f" } } ) );
        REQUIRE( check( "g", { { "@a", "/a.b.c.d.x.g" }, { "@y", "/y.g" } } ) );
        REQUIRE( check( "x", { { "", "/x" }, { "@y", "/y.g.d.x" }, { "@a@e", "/a.b.c.e.x" }, { "@a@f", "/a.b.c.f.x" }, { "@a@d@[0]", "/a.b.c.d.x" }, { "@a@d@[1]", "/a.b.c.d.x.g.d.x" } } ) );

        GIVEN( "/x.$.x (attribute also 'x')" )
        {
            REQUIRE_TRY( anchor::abs_root
                       | view2::child( "x" )
                       | view2::attr
                       | view2::child( "x" )
                       | act2::create_node( km ) );

            // Taking the easy way and ignoring attributes. Perhaps when attrs become navigable, this will need to be altered.
            THEN( "x attribute is ignored" )
            {
                REQUIRE( check( "x", { { "", "/x" }, { "@y", "/y.g.d.x" }, { "@a@e", "/a.b.c.e.x" }, { "@a@f", "/a.b.c.f.x" }, { "@a@d@[0]", "/a.b.c.d.x" }, { "@a@d@[1]", "/a.b.c.d.x.g.d.x" } } ) );
            }
        }
    }
}

} // namespace kmap
