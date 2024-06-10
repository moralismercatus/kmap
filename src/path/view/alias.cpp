/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/alias.hpp"

#include <com/network/network.hpp>
#include <contract.hpp>
#include <kmap.hpp>
#include <path/view/act/fetch_node.hpp>
#include <path/view/act/to_string.hpp>
#include <path/view/ancestor.hpp>
#include <path/view/anchor/node.hpp>
#include <path/view/parent.hpp>
#include <path/view/resolve.hpp>
#include <path/view/tether.hpp>
#include <test/util.hpp>

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

auto Alias::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "this is not what you want" );
}

auto Alias::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred ) -> Result< FetchSet >
            {
                return KTRY( view2::alias( std::string{ pred } ).fetch( ctx, node ) );
            }
        ,   [ & ]( std::string const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( view2::alias.fetch( ctx, node ) );

                return aliases
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( view2::alias.fetch( ctx, node ) );

                return aliases
                     | rvs::filter( [ & ]( auto const& c ){ return c.id == pred; } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( LinkPtr const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( view2::alias.fetch( ctx, node ) );

                return aliases
                     | rvs::filter( [ & ]( auto const& c ){ return anchor::node( c.id ) | pred | act2::exists( ctx.km ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::alias.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( aliases, ns ) // Note: set_intersection requires two sorted ranges.
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
        auto const children = nw->fetch_children( node );

        return children
             | rvs::filter( [ & ]( auto const& c ){ return nw->is_alias( c ); } )
             | rvs::transform( [ & ]( auto const& a ){ return LinkNode{ .id = a }; } )
             | ranges::to< FetchSet >();
    }
}

SCENARIO( "view::Alias::fetch", "[node_view][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "alias" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

        THEN( "view::alias" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( <heading> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( "1" ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( Uuid )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( a21 ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( Link )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( view2::resolve( n1 ) | view2::parent( root ) ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
    }
}

auto Alias::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Alias >();
}

auto Alias::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "alias( '{}' )", arg ); }
        ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "alias( {} )", ( *arg ) | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "alias";
    }
}

auto Alias::compare_less( Link const& other ) const
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

auto AliasSrc::create( CreateContext const& ctx
                     , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "root", root );

    KMAP_ENSURE( pred_, error_code::common::uncategorized );

    auto rv = result::make_result< UuidSet >();
    auto const pred = pred_.value();

    auto dispatch = util::Dispatch
    {
        [ & ]( char const* pred ) -> Result< UuidSet >
        {
            return KTRY( view2::alias_src( std::string{ pred } ).create( ctx, root ) );
        }
    ,   [ & ]( std::string const& pred ) -> Result< UuidSet >
        {
            KMAP_THROW_EXCEPTION_MSG( "inapplicable predicate: " + pred );
        }
    ,   [ & ]( Uuid const& pred ) -> Result< UuidSet >
        {
            auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

            return UuidSet{ KTRY( nw->create_alias( pred, root ) ) };
        }
    ,   [ & ]( UuidSet const& pred ) -> Result< UuidSet >
        {
            auto rv = UuidSet{};
            auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

            for( auto const& pnode : pred )
            {
                rv.emplace( KTRY( nw->create_alias( pnode, root ) ) );
            }

            return rv;
        }
    ,   [ & ]( LinkPtr const& pred ) -> Result< UuidSet >
        {
            KMAP_THROW_EXCEPTION_MSG( "inapplicable predicate; predicate Link probably not what you wanted" );
        }
    ,   [ & ]( Tether const& pred ) -> Result< UuidSet >
        {
            auto rv = UuidSet{};
            auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
            auto const t = pred | act::to_node_set( ctx.km );

            for( auto const& pnode : t )
            {
                rv.emplace( KTRY( nw->create_alias( pnode, root ) ) );
            }

            return rv;
        }
    };

    rv = std::visit( dispatch, pred_.value() );

    return rv;
}

auto AliasSrc::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred ) -> Result< FetchSet >
            {
                return KTRY( view2::alias( std::string{ pred } ).fetch( ctx, node ) );
            }
        ,   [ & ]( std::string const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( view2::alias_src.fetch( ctx, node ) );

                return aliases
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( view2::alias_src.fetch( ctx, node ) );

                return aliases
                     | rvs::filter( [ & ]( auto const& c ){ return c.id == pred; } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( UuidSet const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::alias_src.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = pred
                              | rvs::transform( []( auto const& e ){ return LinkNode{ .id = e }; } )
                              | ranges::to< std::vector< LinkNode > >();

                return rvs::set_intersection( aliases, ns ) // Note: set_intersection requires two sorted ranges.
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( LinkPtr const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( view2::alias_src.fetch( ctx, node ) );

                return aliases
                     | rvs::filter( [ & ]( auto const& c ){ return anchor::node( c.id ) | pred | act2::exists( ctx.km ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const aliases = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::alias_src.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( aliases, ns ) // Note: set_intersection requires two sorted ranges.
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
        auto const children = nw->fetch_children( node );

        return children
             | rvs::filter( [ & ]( auto const& c ){ return nw->is_alias( c ); } )
             | rvs::transform( [ & ]( auto const& a ){ return LinkNode{ .id = nw->resolve( a ) }; } )
             | ranges::to< FetchSet >();
    }
}

SCENARIO( "view::AliasSrc::fetch", "[node_view][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "alias" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

        THEN( "view::alias" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( <heading> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( "1" ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( Uuid )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( a21 ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
        THEN( "view::alias( Link )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( view2::resolve( n1 ) | view2::parent( root ) ) | act::fetch_node( km ) );
            REQUIRE( a21 == fn );
        }
    }
}

auto AliasSrc::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< AliasSrc >();
}

auto AliasSrc::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "alias_src( '{}' )", arg ); }
        ,   [ & ]( UuidSet const& arg ){ return fmt::format( "alias_src( 'TODO<UuidSet>' )" ); }
        ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "alias_src( {} )", ( *arg ) | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "alias_src";
    }
}

auto AliasSrc::compare_less( Link const& other ) const
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