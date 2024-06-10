/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/exactly.hpp>

#include <common.hpp>
#include <path/view/act/to_string.hpp>
#include <path/view/anchor/node.hpp>
#include <path/view/child.hpp>
#include <path/view/common.hpp>
#include <path/view/direct_desc.hpp>
#include <test/util.hpp>
#include <util/result.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

#include <string>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Exactly::create( CreateContext const& ctx
                    , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "TODO: Impl. or N/A" );
    // KM_RESULT_PROLOG();

    // auto rv = result::make_result< UuidSet >();
    // auto rset = UuidSet{};

    // for( auto const& ls = links()
    //    ; auto const& link : ls )
    // {
    //     DerivationLink& dlink = KTRY( result::dyn_cast< DerivationLink >( link.get() ) );
    //     auto const created = KTRY( dlink.create( ctx, root ) );
    //     rset.insert( created.begin(), created.end() );
    // }

    // rv = rset;

    // return rv;
}

auto Exactly::fetch( FetchContext const& ctx
                   , Uuid const& node ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();

    auto rset = FetchSet{};
    auto pred_set = FetchSet{};
    auto unpred_set = FetchSet{};

    for( auto const& ls = links()
       ; auto const& link : ls )
    {
        // Predicated fetch.
        if( auto const fetched = KTRY( anchor::node( node ) | link | act::to_fetch_set( ctx ) )
          ; fetched.size() > 0 )
        {
            pred_set.insert( fetched.begin(), fetched.end() );
        }
        else
        {
            pred_set = {};

            break;
        }

        // Unpredicated fetch.
        auto const us = KTRY( anchor::node( node ) | link->new_link() | act::to_fetch_set( ctx ) );
        unpred_set.insert( us.begin(), us.end() );
    }

    if( pred_set == unpred_set )
    {
        rset = pred_set;
    }

    return rset;
}

SCENARIO( "view2::Exactly::fetch", "[node_view][exactly]" )
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

        REQUIRE(( anchor::node( root ) | view2::exactly( view2::child, std::set< std::string >{ "1", "2", "3" } ) | act2::exists( km ) ));

        THEN( "exactly( child, '1', '2', '3' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::exactly( view2::child, std::set< std::string >{ "1", "2", "3" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 3 );
            REQUIRE( matches.contains( c1 ) );
            REQUIRE( matches.contains( c2 ) );
            REQUIRE( matches.contains( c3 ) );
        }
        THEN( "exactly( child, '1', '2', '3', '4' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::exactly( view2::child, std::set< std::string >{ "1", "2", "3", "4" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 0 );
        }
        THEN( "exactly( child, '1', '2', '4' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::exactly( view2::child, std::set< std::string >{ "1", "2", "3", "4" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 0 );
        }
        THEN( "exactly( child, '1' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::exactly( view2::child, std::set< std::string >{ "1" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 0 );
        }
        THEN( "exactly( child, '1' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::exactly( view2::child, std::set< std::string >{ "1" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 0 );
        }
        THEN( "exactly( child, '1', '2' )" )
        {
            auto const matches = anchor::node( root )
                               | view2::exactly( view2::child, std::set< std::string >{ "1", "2" } )
                               | act2::to_node_set( km );

            REQUIRE( matches.size() == 0 );
        }
    }
}

auto Exactly::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Exactly >();
}

auto Exactly::to_string() const
    -> std::string
{
    if( auto const& ls = links()
      ; !ls.empty() )
    {
        auto const lstrs = links()
                         | rvs::transform( [ & ]( auto const& lp ){ return *lp | act::to_string; } )
                         | rvs::join( ',' )
                         | ranges::to< std::string >();

        return fmt::format( "exactly( {} )", lstrs );
    }
    else
    {
        return "exactly";
    }
}

auto Exactly::compare_less( Link const& other ) const
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

SCENARIO( "Exactly::compare_less", "[node_view][link]")
{
    GIVEN( "anchor::node( id ) | view2::exactly( view2::direct_desc, { <str>, <str>, <str> } )" )
    {
        auto const acn = gen_uuid();
        auto const t1_ct = anchor::node( acn )
                         | view2::exactly( view2::direct_desc
                                        , { "subject.sierra", "verb.victor", "object.oscar" } );
        auto const t1 = t1_ct | to_tether;
        auto const t2 = t1_ct | to_tether;
        auto const t3_ct = anchor::node( acn )
                         | view2::exactly( view2::direct_desc
                                        , { "subject.sierra", "verb.victor", "object.oscar" } );
        auto const t3 = t3_ct | to_tether;
        
        REQUIRE( !( t1 < t1 ) );
        REQUIRE( !( t2 < t2 ) );
        REQUIRE( !( t3 < t3 ) );
        REQUIRE(( !( t1 < t2 ) && !( t2 < t1 ) ));
        REQUIRE(( !( t1 < t3 ) && !( t3 < t1 ) ));
    }
}

} // namespace kmap::view2
