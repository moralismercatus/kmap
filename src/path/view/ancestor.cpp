/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/ancestor.hpp>

#include <com/network/network.hpp>
#include <path.hpp>
#include <path/ancestry.hpp>
#include <test/util.hpp>
#include <utility.hpp>

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

auto Ancestor::create( CreateContext const& ctx
                     , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "this is not what you want" );
}

auto Ancestor::fetch( FetchContext const& ctx
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
                return KTRY( view2::ancestor( std::string{ pred } ).fetch( ctx, node ) );
            }
        ,   [ & ]( std::string const& pred ) -> Result< FetchSet >
            {
                auto rs = FetchSet{};
                auto const ancestors = KTRY( view2::ancestor.fetch( ctx, node ) );
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

                return ancestors
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

                if( is_ancestor( *nw, pred, node ) )
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
                auto const ancestors = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::ancestor.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( anchor::node( node ) | pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( ancestors, ns )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const ancestors = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( view2::ancestor.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > >
                {
                    auto t = KTRY( pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( ancestors, ns ) // Note: set_intersection requires two sorted ranges.
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const rs = KTRY( [ & ] -> Result< FetchSet >
        {
            auto const ancestry = KTRY( path::fetch_ancestry_ordered( ctx.km, node ) );
            return ancestry
                 | rvs::transform( []( auto const& n ){ return LinkNode{ .id = n }; } )
                 | ranges::to< FetchSet >();
        }() );

        return rs;
    }
}

SCENARIO( "view::Ancestor::fetch", "[node_view][ancestor]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "ancestor" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

        THEN( "view::ancestor" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::ancestor | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::ancestor( <heading> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::ancestor( "root" ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::ancestor( Uuid )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::ancestor( root ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
        THEN( "view::ancestor( <Tether> )" )
        {
            auto const fn = REQUIRE_TRY( anchor::node( n1 ) | view2::ancestor( anchor::node( n1 ) | view2::parent ) | act::fetch_node( km ) );
            REQUIRE( root == fn );
        }
    }
}

auto Ancestor::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Ancestor >();
}

auto Ancestor::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& pred ){ return fmt::format( "ancestor( '{}' )", pred ); }
        ,   [ & ]( LinkPtr const& pred ){ return fmt::format( "ancestor( '{}' )", *pred | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "ancestor";
    }
}

auto Ancestor::compare_less( Link const& other ) const
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
