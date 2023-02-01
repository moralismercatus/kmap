/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/all_of.hpp"

#include "common.hpp"
#include "path/view/act/to_string.hpp"
#include "path/view/anchor/node.hpp"
#include "path/view/child.hpp"
#include "path/view/common.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

#include <string>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto AllOf::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< UuidSet >();
    auto rset = UuidSet{};

    for( auto const& ls = links()
       ; auto const& link : ls )
    {
        auto const created = KTRY( link->create( ctx, root ) );
        rset.insert( created.begin(), created.end() );
    }

    rv = rset;

    return rv;
}

auto AllOf::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> FetchSet
{
    auto rset = FetchSet{};

    for( auto const& ls = links()
       ; auto const& link : ls )
    {
        
        if( auto const fetched = anchor::node( node ) | link | act::to_fetch_set( ctx )
          ; fetched.size() > 0 )
        {
            rset.insert( fetched.begin(), fetched.end() );
        }
        else
        {
            rset = {};

            break;
        }
    }

    return rset;
}

SCENARIO( "view::AllOf::fetch", "[node_view][all_of]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "child( 1, 2, 3 )" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const c3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

        REQUIRE(( anchor::node( root ) | view2::all_of( view2::child, std::set< std::string >{ "1", "2", "3" } ) | act2::exists( km ) ));

        THEN( "all_of( child, '1', '2', '3' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::all_of( view2::child, std::set< std::string >{ "1", "2", "3" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 3 );
            REQUIRE( matches.contains( c1 ) );
            REQUIRE( matches.contains( c2 ) );
            REQUIRE( matches.contains( c3 ) );
        }
        THEN( "all_of( child, '1', '2', '3', '4' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::all_of( view2::child, std::set< std::string >{ "1", "2", "3", "4" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 0 );
        }
        THEN( "all_of( child, '1', '2', '4' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::all_of( view2::child, std::set< std::string >{ "1", "2", "3", "4" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 0 );
        }
    }
}

auto AllOf::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< AllOf >();
}

auto AllOf::to_string() const
    -> std::string
{
    if( auto const& ls = links()
      ; !ls.empty() )
    {
        auto const lstrs = links()
                         | rvs::transform( [ & ]( auto const& lp ){ return *lp | act::to_string; } )
                         | rvs::join( ',' )
                         | ranges::to< std::string >();

        return fmt::format( "all_of( {} )", lstrs );
    }
    else
    {
        return "all_of";
    }
}

auto AllOf::compare_less( Link const& other ) const
    -> bool
{
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return links() < dynamic_cast< decltype( *this )& >( other ).links();
    }
    else if( cmp == std::strong_ordering::less )
    {
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace kmap::view2
