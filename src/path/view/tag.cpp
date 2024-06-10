/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/tag.hpp>

#include <com/network/network.hpp>
#include <com/tag/tag.hpp>
#include <contract.hpp>
#include <path/view/alias.hpp>
#include <path/view/attr.hpp>
#include <path/view/child.hpp>
#include <path/view/common.hpp>
#include <util/result.hpp>

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

auto Tag::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
    
    auto rv = result::make_result< UuidSet >();

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred ) -> Result< UuidSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const c = KTRY( nw->create_tag( root, pred ) );

                return UuidSet{ c };
            }
        ,   [ & ]( std::string const& pred ) -> Result< UuidSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const c = KTRY( nw->create_tag( root, pred ) );

                return UuidSet{ c };
            }
        ,   [ & ]( Uuid const& pred ) -> Result< UuidSet >
            {
                KMAP_THROW_EXCEPTION_MSG( "invalid node qualifier" );
            }
        };

        rv = KTRY( std::visit( dispatch, pred_.value() ) );
    }

    return rv;
}

auto Tag::fetch( FetchContext const& ctx
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
                return KTRY( view2::tag( std::string{ pred } ).fetch( ctx, node ) );
            }
        ,   [ & ]( std::string const& pred ) -> Result< FetchSet >
            {
                auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );
                auto const tags = KTRY( view2::tag.fetch( ctx, node ) );

                return tags 
                     | rvs::filter( [ & ]( auto const& e ){ return pred == KTRYE( nw->fetch_heading( e.id ) ); } )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Uuid const& pred ) -> Result< FetchSet >
            {
                auto rv = FetchSet{};
                auto const tag_dsts = KTRY( view2::tag.fetch( ctx, node ) );

                for( auto const& t : tag_dsts )
                {
                    if( pred == resolve( t.id ) )
                    {
                        rv.emplace( t );
                        break;
                    }
                }

                return rv;
            }
        ,   [ & ]( UuidSet const& pred ) -> Result< FetchSet >
            {
                auto tag_dsts_v = KTRY( view2::tag.fetch( ctx, node ) ) | ranges::to< std::vector >();
                auto pred_v = anchor::node( pred ) 
                            | view2::resolve( view2::ancestor( view2::tag::tag_root ) )
                            | act::to_node_vec( ctx.km );

                ranges::sort( tag_dsts_v );
                ranges::sort( pred_v );

                return rvs::set_intersection( tag_dsts_v, pred_v )
                     | ranges::to< FetchSet >();
            }
        ,   [ & ]( Tether const& pred ) -> Result< FetchSet >
            {
                auto const pred_set = pred | act::to_node_set( ctx );

                return KTRY( view2::tag.fetch( ctx, pred_set ) );
            }
        ,   [ & ]( Tether const& pred ) -> Result< FetchSet >
            {
                auto const resolutions = KTRY( [ & ]() -> Result< std::vector< LinkNode > > // Note: set_intersection requires two sorted ranges.
                {
                    auto t = KTRY( view2::resolve.fetch( ctx, node ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );
                auto const ns = KTRY( [ & ]() -> Result< std::vector< LinkNode > > // Note: set_intersection requires two sorted ranges.
                {
                    auto t = KTRY( pred | act::to_fetch_set( ctx ) ) | ranges::to< std::vector >();
                    ranges::sort( t );
                    return t;
                }() );

                return rvs::set_intersection( resolutions, ns )
                     | ranges::to< FetchSet >();
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return anchor::node( node )
             | view2::attr
             | view2::child( "tag" )
             | view2::alias
             | view2::resolve( view2::ancestor( view2::tag::tag_root ) )
             | act::to_fetch_set( ctx );
    }
}

auto Tag::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Tag >();
}

auto Tag::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "tag( '{}' )", arg ); }
        ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "tag( {} )", ( *arg ) | act::to_string ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "tag";
    }
}

auto Tag::compare_less( Link const& other ) const
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

SCENARIO( "Tag::compare_less", "[node_view]" )
{
    GIVEN( "two tag views defined with same predicates" )
    {
        auto const c1 = view2::tag( "1" );
        auto const c1b = view2::tag( "1" );
        Link const& l1 = c1;

        REQUIRE( !( c1 < c1 ) );
        REQUIRE( !( c1 < c1b ) );
        REQUIRE( !( c1b < c1 ) );
        REQUIRE( !( l1 < c1 ) );
        REQUIRE( !( l1 < c1b ) );
        REQUIRE( !( c1 < l1 ) );
        REQUIRE( !( c1b < l1 ) );
    }
}

} // namespace kmap::view2