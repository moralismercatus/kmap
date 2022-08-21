/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "abs_path.hpp"

#include "com/database/root_node.hpp"
#include "com/network/network.hpp"
#include "common.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/reverse.hpp>

#include <vector>

namespace kmap::view::act {

AbsPath::AbsPath( Kmap const& kmap )
    : km{ kmap }
{
}

AbsPathFlat::AbsPathFlat( Kmap const& kmap )
    : km{ kmap }
{
}

auto abs_path( Kmap const& km )
    -> AbsPath
{
    return AbsPath{ km };
}

auto abs_path_flat( Kmap const& km )
    -> AbsPathFlat
{
    return AbsPathFlat{ km };
}

auto operator|( Intermediary const& i, AbsPath const& op )
    -> Result< UuidVec >
{
    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const nw = KTRY( op.km.fetch_component< com::Network >() );
    auto const lhs = KTRY( [ & ] -> Result< Uuid >
    {
        if( i.root.size() == 1 )
        {
            return { *i.root.begin() };
        }
        else
        {
            return KMAP_MAKE_ERROR( error_code::common::uncategorized );
        }
    }() );
    auto const rhs = KTRY( [ & ] -> Result< Uuid >
    {
        if( auto const ns = i | view::to_node_set( op.km )
          ; ns.size() == 1 )
        {
            return { *ns.begin() };
        }
        else
        {
            return KMAP_MAKE_ERROR( error_code::common::uncategorized );
        }
    }() );
    auto const [ root, desc ] = KTRY( [ & ] -> Result< std::pair< Uuid, Uuid > >
    {
        if( is_ancestor( op.km, lhs, rhs ) )
        {
            return { lhs, rhs };
        }
        else if( is_ancestor( op.km, rhs, lhs ) )
        {
            return { rhs, lhs };
        }
        else if( lhs == rhs )
        {
            return { lhs, rhs };
        }
        else
        {
            return KMAP_MAKE_ERROR( error_code::common::uncategorized );
        }
    }() );
    auto path = std::vector< Uuid >{};

    auto current = desc;

    while( current != root )
    {
        path.emplace_back( current );

        current = KTRYE( nw->fetch_parent( current ) );
    }

    path.emplace_back( current );

    rv = path | ranges::views::reverse | ranges::to< UuidVec >();

    return rv;
}

SCENARIO( "act::abs_path", "[node_view][abs_path]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network", "alias_store" );

    auto& km = Singleton::instance();
    auto const rn = REQUIRE_TRY( km.fetch_component< com::RootNode >() );
    auto const root = rn->root_node();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );

    GIVEN( "node tree /[1.2.3]|[4.5.6]" )
    {
        REQUIRE_TRY( view::make( root )
                   | view::direct_desc( "1.2.3" )
                   | view::create_node( km ) );
        REQUIRE_TRY( view::make( root )
                   | view::direct_desc( "4.5.6" )
                   | view::create_node( km ) );

        auto const flatten = [ & ]( auto const& uv ) -> std::string
        {
            return uv 
                 | ranges::views::transform( [ & ]( auto const& e ){ return KTRYE( nw->fetch_heading( e ) ); } )
                 | ranges::views::join( '.' )
                 | ranges::to< std::string >();
        };

        THEN( "[1, 2] => 1.2" )
        {
            auto const n1 = REQUIRE_TRY( view::make( root )
                                       | view::desc( "1" )
                                       | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( n1 )
                                      | view::desc( "2" )
                                      | act::abs_path( km ) );

            REQUIRE( flatten( r ) == "1.2" );
        }
        THEN( "[1, 3] => 1.2.3" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "1" )
                                        | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( lhs )
                                      | view::desc( "3" )
                                      | act::abs_path( km ) );

            REQUIRE( flatten( r ) == "1.2.3" );
        }
        THEN( "[2, 3] => 2.3" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "2" )
                                        | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( lhs )
                                      | view::desc( "3" )
                                      | act::abs_path( km ) );

            REQUIRE( flatten( r ) == "2.3" );
        }
        THEN( "[/, 3] => root.1.2.3" )
        {
            auto const r = REQUIRE_TRY( view::make( root )
                                      | view::desc( "3" )
                                      | act::abs_path( km ) );

            REQUIRE( flatten( r ) == "root.1.2.3" );
        }
        THEN( "[3, /] => root.1.2.3" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "3" )
                                        | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( lhs )
                                      | view::ancestor( root )
                                      | act::abs_path( km ) );

            REQUIRE( flatten( r ) == "root.1.2.3" );
        }
        THEN( "[3, 6] => error" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "3" )
                                        | view::fetch_node( km ) );

            REQUIRE( test::fail( view::make( lhs )
                               | view::ancestor( root )
                               | view::direct_desc( "6" )
                               | act::abs_path( km ) ) );
        }
        THEN( "[/] => root" )
        {
            auto const r = REQUIRE_TRY( view::make( root )
                                      | act::abs_path( km ) );

            REQUIRE( flatten( r ) == "root" );
        }
    }
}

auto operator|( Intermediary const& i, AbsPathFlat const& op )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const rn = KTRY( op.km.fetch_component< com::RootNode >() );
    auto const nw = KTRY( op.km.fetch_component< com::Network >() );
    auto const root = rn->root_node();
    auto const ap = KTRY( i | abs_path( op.km ) );

    auto const joined = ap
                      | ranges::views::transform( [ & ]( auto const& e ){ return ( e == root ) ? std::string{ "/" } : KTRYE( nw->fetch_heading( e ) ); } )
                      | ranges::views::join( '.' )
                      | ranges::to< std::string >();

    rv = boost::algorithm::replace_first_copy( joined, "/.", "/" );

    return rv;
}

SCENARIO( "act::abs_path_flat", "[node_view][abs_path]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network", "alias_store" );

    auto& km = Singleton::instance();
    auto const rn = REQUIRE_TRY( km.fetch_component< com::RootNode >() );
    auto const root = rn->root_node();

    GIVEN( "node tree /[1.2.3]|[4.5.6]" )
    {
        REQUIRE_TRY( view::make( root )
                   | view::direct_desc( "1.2.3" )
                   | view::create_node( km ) );
        REQUIRE_TRY( view::make( root )
                   | view::direct_desc( "4.5.6" )
                   | view::create_node( km ) );

        THEN( "[1, 2] => 1.2" )
        {
            auto const n1 = REQUIRE_TRY( view::make( root )
                                       | view::desc( "1" )
                                       | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( n1 )
                                      | view::desc( "2" )
                                      | act::abs_path_flat( km ) );

            REQUIRE( r == "1.2" );
        }
        THEN( "[1, 3] => 1.2.3" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "1" )
                                        | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( lhs )
                                      | view::desc( "3" )
                                      | act::abs_path_flat( km ) );

            REQUIRE( r == "1.2.3" );
        }
        THEN( "[2, 3] => 2.3" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "2" )
                                        | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( lhs )
                                      | view::desc( "3" )
                                      | act::abs_path_flat( km ) );

            REQUIRE( r == "2.3" );
        }
        THEN( "[/, 3] => /1.2.3" )
        {
            auto const r = REQUIRE_TRY( view::make( root )
                                      | view::desc( "3" )
                                      | act::abs_path_flat( km ) );

            REQUIRE( r == "/1.2.3" );
        }
        THEN( "[3, /] => /1.2.3" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "3" )
                                        | view::fetch_node( km ) );
            auto const r = REQUIRE_TRY( view::make( lhs )
                                      | view::ancestor( root )
                                      | act::abs_path_flat( km ) );

            REQUIRE( r == "/1.2.3" );
        }
        THEN( "[3, 6] => error" )
        {
            auto const lhs = REQUIRE_TRY( view::make( root )
                                        | view::desc( "3" )
                                        | view::fetch_node( km ) );

            REQUIRE( test::fail( view::make( lhs )
                               | view::ancestor( root )
                               | view::direct_desc( "6" )
                               | act::abs_path_flat( km ) ) );
        }
        THEN( "[/] => /" )
        {
            auto const r = REQUIRE_TRY( view::make( root )
                                      | act::abs_path_flat( km ) );

            REQUIRE( r == "/" );
        }
    }
}

} // namespace kmap::view::act