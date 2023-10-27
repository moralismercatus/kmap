/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/desc.hpp"

#include "com/network/network.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "path/act/value_or.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Desc::create( CreateContext const& ctx
                 , Uuid const& root ) const
    -> Result< UuidSet >
{
    return result::make_result< UuidSet >();
}

auto Desc::fetch( FetchContext const& ctx
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
                return KTRY( view2::desc( std::string{ pred } ).fetch( ctx, node ) );
            }
        ,   [ & ]( std::string const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const dns = decide_path( ctx.km, node, node, pred ) | view::act::value_or( UuidVec{} );
                auto const tform = [ & ]( auto const& c )
                {
                    return LinkNode{ .id = c }; 
                };
                return dns 
                     | rvs::transform( tform )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

                if( is_ancestor( *nw, node, pred ) )
                {
                    return FetchSet{ LinkNode{ .id = pred  } };
                }
                else
                {
                    return FetchSet{};
                }
            }
        ,   [ & ]( LinkPtr const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const descs = KTRY( desc.fetch( ctx, node ) );

                return descs 
                     | rvs::filter( [ & ]( auto const& c ){ return anchor::node( c.id ) | pred | act2::exists( ctx.km ); } )
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
        auto const descs = KTRY( fetch_descendants( ctx.km, node ) );

        return descs
             | rvs::transform( []( auto const& c ){ return LinkNode{ .id = c }; } )
             | ranges::to< FetchSet >();
    }
}

SCENARIO( "view::Desc::fetch", "[node_view][desc]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    // auto& km = Singleton::instance();
    // auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    // auto const root = nw->root_node();

    // GIVEN( "alias" )
    // {
    //     auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
    //     auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
    //     auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

    //     THEN( "view::alias" )
    //     {
    //         auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias | act::fetch_node( km ) );
    //         REQUIRE( a21 == fn );
    //     }
    //     THEN( "view::alias( <heading> )" )
    //     {
    //         auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( "1" ) | act::fetch_node( km ) );
    //         REQUIRE( a21 == fn );
    //     }
    //     THEN( "view::alias( Uuid )" )
    //     {
    //         auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( a21 ) | act::fetch_node( km ) );
    //         REQUIRE( a21 == fn );
    //     }
    //     THEN( "view::alias( <Chain> )" )
    //     {
    //         auto const fn = REQUIRE_TRY( anchor::node( n2 ) | view2::alias( view2::resolve( n1 ) | view2::parent( root ) ) | act::fetch_node( km ) );
    //         REQUIRE( a21 == fn );
    //     }
    // }
}

auto Desc::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Desc >();
}

auto Desc::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "desc( '{}' )", arg ); }
        ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "desc( {} )", ( *arg ) | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "desc";
    }
}

auto Desc::compare_less( Link const& other ) const
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