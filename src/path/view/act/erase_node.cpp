/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/erase_node.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/fetch_node.hpp"
#include "path/view/act/to_node_set.hpp"
#include "path/view/act/to_string.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::view2::act {

EraseNode::EraseNode( Kmap& k )
    : km{ k }
{
}

auto erase_node( Kmap& km )
    -> EraseNode 
{
    return EraseNode{ km };
}

auto operator|( Tether const& lhs
              , EraseNode const& rhs )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const nw = KTRY( rhs.km.fetch_component< com::Network >() );
    auto const nodes = lhs | to_node_set( rhs.km );

    for( auto const& node : nodes )
    {
        KTRY( nw->erase_node( node ) );
    }

    rv = outcome::success();

    return rv;
}

SCENARIO( "act::erase_node", "[node_view][action]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/" )
    {
        WHEN( "erase_node( /1 )" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            REQUIRE_TRY( anchor::node( root )
                       | view2::child( "1" )
                       | act::erase_node( km ) );
            
            THEN( "/1 erased" )
            {
                REQUIRE( !( anchor::node( n1 ) | act::exists( km ) ) );
            }
        }
    }
}

} // namespace kmap::view2::act
