/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/fetch_or_create_node.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/fetch_node.hpp"
#include "path/view/act/to_node_set.hpp"
#include "path/view/act/to_string.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::view2::act {

FetchOrCreateNode::FetchOrCreateNode( Kmap& k )
    : km{ k }
{
}

auto fetch_or_create_node( Kmap& km )
    -> FetchOrCreateNode 
{
    return FetchOrCreateNode{ km };
}

auto operator|( Tether const& lhs
              , FetchOrCreateNode const& rhs )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );

    if( auto const fn = lhs | act::fetch_node( rhs.km )
      ; fn )
    {
        rv = fn.value();
    }
    else
    {
        auto const fctx = FetchContext{ rhs.km, lhs };
        auto ns = lhs.anchor->fetch( fctx );
        auto const links = [ & ]
        {
            auto rlinks = std::deque< Link::LinkPtr >{};
            auto link = lhs.tail_link;
            while( link )
            {
                rlinks.emplace_front( link );
                link = link->prev();
            }
            return rlinks;
        }();

        for( auto const& link : links )
        {
            auto const nns = KTRY( [ & ]() -> Result< FetchSet >
            {
                auto tnns = decltype( ns ){};
                for( auto const& n : ns )
                {
                    auto const fr = KTRY( fetch( link, fctx, n.id ) );
                    tnns.insert( fr.begin(), fr.end() );
                }
                return tnns;
            }() );

            if( nns.empty() )
            {
                // TODO: Should this actually create based on all `ns` nodes? Rename to fetch_or_create, and force user to use view::single | fetch_or_create if
                //       user desires only a single output.
                KMAP_ENSURE( ns.size() == 1, error_code::common::uncategorized );

                DerivationLink& dlink = KTRY( result::dyn_cast< DerivationLink >( link.get() ) );
                auto const cs = KTRY( dlink.create( CreateContext{ rhs.km, fctx.tether }, ns.begin()->id ) );

                ns = cs
                   | rvs::transform( [ & ]( auto const& e ){ return LinkNode{ .id = e }; } )
                   | ranges::to< FetchSet >();
            }
            else
            {
                ns = nns;
            }
        }

        BC_ASSERT( ns.size() == 1 );

        rv = ns.begin()->id;
    }

    return rv;
}

SCENARIO( "act::fetch_or_create_node", "[node_view][action]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/" )
    {
        WHEN( "foc( /1 )" )
        {
            auto const n1 = REQUIRE_TRY( anchor::node( root )
                                       | view2::child( "1" )
                                       | act::fetch_or_create_node( km ) );
            
            THEN( "/1 created" )
            {
                REQUIRE(( anchor::node( n1 ) | view2::parent( root ) | act::exists( km ) ));
            }
        }

        GIVEN( "/1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            WHEN( "foc( /1 )" )
            {
                auto const res = REQUIRE_TRY( anchor::node( root )
                                            | view2::child( "1" )
                                            | act::fetch_or_create_node( km ) );

                REQUIRE( res == n1 );
            }
        }
    }
}

} // namespace kmap::view2::act
