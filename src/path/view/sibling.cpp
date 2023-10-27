/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/sibling.hpp"

#include "com/network/network.hpp"
#include "common.hpp"
#include "path/view/anchor/node.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto SiblingIncl::create( CreateContext const& ctx
                        , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "TODO" );
}

auto SiblingIncl::fetch( FetchContext const& ctx
                       , Uuid const& node ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred ) -> Result< FetchSet >
            {
                return KTRY( view2::sibling_incl( std::string{ pred } ).fetch( ctx, node ) );
            }
        ,   [ & ]( std::string const& pred ) -> Result< FetchSet >
            {
                auto const sibling_incls = KTRY( view2::sibling_incl.fetch( ctx, node ) );
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

                return sibling_incls
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred ) -> Result< FetchSet >
            {
                auto const sibling_incls = KTRY( view2::sibling_incl.fetch( ctx, node ) );

                if( sibling_incls.contains( pred ) )
                {
                    return FetchSet{ LinkNode{ .id = pred } };
                }
                else
                {
                    return FetchSet{};
                }
            }
        ,   [ & ]( LinkPtr const& pred ) -> Result< FetchSet >
            {
                auto const sibling_incls = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::sibling_incl.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( anchor::node( node ) | pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( sibling_incls, ns ) // Note: set_intersection requires two sorted ranges.
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const sibling_incls = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::sibling_incl.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( sibling_incls, ns ) // Note: set_intersection requires two sorted ranges.
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
        auto fs = FetchSet{};
        
        if( auto const parent = nw->fetch_parent( node )
          ; parent )
        {
            auto const children = nw->fetch_children( parent.value() );
            
            fs = children
               | rvs::transform( []( auto const& e ){ return LinkNode{ .id = e }; } )
               | ranges::to< FetchSet >();
        }
        else // node is root.
        {
            fs = FetchSet{ LinkNode{ .id = node } };
        }

        return fs;
    }
}

SCENARIO( "view::SiblingIncl::fetch", "[node_view][sibling_incl]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/" )
    {
        THEN( "si: /" )
        {
            auto const ns = anchor::node( root ) | view2::sibling_incl | act::to_node_set( km );
            REQUIRE( ns == UuidSet{ root } );
        }
    }

    GIVEN( "sibling_incl" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

        THEN( "view::sibling_incl" )
        {
            auto const fn = anchor::node( n1 ) | view2::sibling_incl | act::to_node_set( km );
            REQUIRE( fn == UuidSet{ n1, n2 } );
        }
        THEN( "view::sibling_incl( <heading> )" )
        {
            auto const fn1 = REQUIRE_TRY( anchor::node( n1 ) | view2::sibling_incl( "1" ) | act::fetch_node( km ) );
            REQUIRE( n1 == fn1 );
            auto const fn2 = REQUIRE_TRY( anchor::node( n1 ) | view2::sibling_incl( "2" ) | act::fetch_node( km ) );
            REQUIRE( n2 == fn2 );
        }
        THEN( "view::sibling_incl( Uuid )" )
        {
            auto const fn1 = REQUIRE_TRY( anchor::node( n1 ) | view2::sibling_incl( n1 ) | act::fetch_node( km ) );
            REQUIRE( n1 == fn1 );
            auto const fn2 = REQUIRE_TRY( anchor::node( n1 ) | view2::sibling_incl( n2 ) | act::fetch_node( km ) );
            REQUIRE( n2 == fn2 );
        }
        THEN( "view::sibling_incl( <Tether> )" )
        {
            auto const fn = anchor::node( n1 ) | view2::sibling_incl( anchor::node( root ) | view2::child ) | act::to_node_set( km );
            REQUIRE( fn == UuidSet{ n1, n2 } );
        }
    }
}

auto SiblingIncl::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< SiblingIncl >();
}

auto SiblingIncl::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& pred ){ return fmt::format( "sibling_incl( '{}' )", pred ); }
        ,   [ & ]( LinkPtr const& pred ){ return fmt::format( "sibling_incl( '{}' )", *pred | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "sibling_incl";
    }
}

auto SiblingIncl::compare_less( Link const& other ) const
    -> bool
{ 
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return pred_ < dynamic_cast< decltype( *this )& >( other ).pred_;
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
