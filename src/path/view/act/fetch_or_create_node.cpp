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
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( UuidSet );

    if( auto const fn = lhs | act::to_node_set( rhs.km )
      ; !fn.empty() )
    {
        rv = fn;
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
                // TODO: One major difficulty: what happens if create fails? The whole operation should undo - but I don't have facilities for that at this point.

                DerivationLink& dlink = KTRY( result::dyn_cast< DerivationLink >( link.get() ) );

                KMAP_ENSURE( ns.size() == 1, error_code::common::uncategorized ); // TODO: Is it true that ns.size always makes sense to be 1?

                auto const cctx = CreateContext{ .km = rhs.km
                                               , .tether = fctx.tether
                                               , .option = { .skip_existing = true }  };
                auto const cs = KTRY( dlink.create( cctx, ns.begin()->id ) );

                ns = cs
                   | rvs::transform( [ & ]( auto const& e ){ return LinkNode{ .id = e }; } )
                   | ranges::to< FetchSet >();
            }
            else
            {
                ns = nns;
            }
        }

        rv = ns
           | rvs::transform( &LinkNode::id )
           | ranges::to< UuidSet >();
    }

    return rv;
}

SCENARIO( "act::fetch_or_create_node", "[nodVe_view][action]" )
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
                                       | act::fetch_or_create_node( km )
                                       | act::single );
            
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
                                            | act::fetch_or_create_node( km )
                                            | act::single );

                REQUIRE( res == n1 );
            }

            THEN( "foc( all_of( /1, /2 ) )")
            {
                auto const res = REQUIRE_TRY( anchor::node( root )
                                            | view2::all_of( view2::child( "1" )
                                                           , view2::child( "2" ) )
                                            | act2::fetch_or_create_node( km ) );
                auto const n2 = REQUIRE_TRY( anchor::node( root )
                                           | view2::child( "2" )
                                           | act2::fetch_node( km ) );

                REQUIRE( res.contains( n2 ) );
            }

            GIVEN( "/2" )
            {
                auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
                auto const res = REQUIRE_TRY( anchor::node( root )
                                            | view2::all_of( view2::child( "1" )
                                                           , view2::child( "2" ) )
                                            | act2::fetch_or_create_node( km ) );

                REQUIRE( res.contains( n1 ) );
                REQUIRE( res.contains( n2 ) );
            }
        }
    }
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
                                       | act::fetch_or_create_node( km )
                                       | act::single );
            
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
                                            | act::fetch_or_create_node( km )
                                            | act::single );

                REQUIRE( res == n1 );
            }
        }
    }
}

} // namespace kmap::view2::act
