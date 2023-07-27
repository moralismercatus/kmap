/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/root.hpp"

#include "com/network/network.hpp"
#include "path.hpp"
#include "test/util.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

#include <compare>
#include <variant>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Root::create( CreateContext const& ctx
                 , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "this is not what you want" );
}

auto Root::fetch( FetchContext const& ctx
                , Uuid const& node ) const
    -> FetchSet
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred )
            {
                return view2::root( std::string{ pred } ).fetch( ctx, node );
            }
        ,   [ & ]( std::string const& pred )
            {
                auto rs = FetchSet{};
                auto const roots = view2::root.fetch( ctx, node );
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );

                return roots
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred )
            {
                auto const root = view2::root.fetch( ctx, node );
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );

                if( root.contains( pred ) )
                {
                    return FetchSet{ LinkNode{ .id = pred } };
                }
                else
                {
                    return FetchSet{};
                }
            }
        ,   [ & ]( LinkPtr const& pred )
            {
                auto const roots = [ & ]
                {
                    auto t = view2::root.fetch( ctx, node ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();
                auto const ns = [ & ] 
                {
                    auto t = anchor::node( node ) | pred | act::to_fetch_set( ctx ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();

                return rvs::set_intersection( roots, ns )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const roots = [ & ]
                {
                    auto t = view2::root.fetch( ctx, node ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();
                auto const ns = [ & ]
                {
                    auto t = pred | act::to_fetch_set( ctx ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }();

                return rvs::set_intersection( roots, ns ) // Note: set_intersection requires two sorted ranges.
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto rs = FetchSet{};
        auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
        auto it_node = nw->fetch_parent( node );
        auto child = node;

        while( it_node )
        {
            child = it_node.value();

            it_node = nw->fetch_parent( child );
        }

        rs.emplace( LinkNode{ .id = child } );

        return rs;
    }
}

SCENARIO( "view::Root::fetch", "[node_view][root][attribute]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/1" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

        THEN( "view::root" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::root | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::root( <heading> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::root( "root" ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::root( Uuid )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::root( root ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::root( <Tether> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::root( anchor::node( n1 ) | view2::parent ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }

        GIVEN( "/1.$.a" )
        {
            auto const a = REQUIRE_TRY( anchor::node( n1 ) | view2::attr | view2::child( "a" ) | act2::create_node( km ) );
            auto const attr_root = REQUIRE_TRY( anchor::node( n1 ) | view2::attr | act2::fetch_node( km ) );

            THEN( "view::root" )
            {
                auto const fn = REQUIRE_TRY( anchor::node( a ) | view2::root | act::fetch_node( km ) );
                REQUIRE( attr_root == fn );
            }

            GIVEN( "/1.$.a.b" )
            {
                auto const b = REQUIRE_TRY( anchor::node( a ) | view2::child( "b" ) | act2::create_node( km ) );

                THEN( "view::root" )
                {
                    auto const fn = REQUIRE_TRY( anchor::node( b ) | view2::root | act::fetch_node( km ) );
                    REQUIRE( attr_root == fn );
                }
            }
        }
    }
}

auto Root::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Root >();
}

auto Root::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& pred ){ return fmt::format( "root( '{}' )", pred ); }
        ,   [ & ]( LinkPtr const& pred ){ return fmt::format( "root( '{}' )", *pred | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "root";
    }
}

auto Root::compare_less( Link const& other ) const
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
