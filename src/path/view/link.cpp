/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/link.hpp"

#include "common.hpp"
#include "path/view/child.hpp"
#include "path/view/desc.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace kmap::view2 {

bool Link::operator<( Link const& other ) const
{
    KMAP_LOG_LINE();
    auto const tp = prev();
    auto const op = other.prev();

    if( tp && op )
    {
        if( !( *tp < *op ) && !( *op < *tp ) )
        {
            return compare_less( other );
        }
        else
        {
            return tp < op;
        }
    }
    else if( !tp && !op )
    {
        return compare_less( other );
    }
    else
    {
        return tp < op;
    }
}

SCENARIO( "view::Link::operator<", "[node_view][link]" )
{
    // case: single link comparison
    // case: multi link comparison
    // case: predicated multi link comparison

    auto const is_same = []( auto const& lhs, auto const& rhs )
    {
        return !( lhs < rhs ) && !( rhs < lhs );
    };

    THEN( "global unpredicated links are same" )
    {
        // REQUIRE( is_same( view2::child, view2::child ) );
    }
    THEN( "base-referenced unpredicated links are same" )
    {
        Link const& c1 = view2::child;
        Link const& c2 = view2::child;

        // REQUIRE( is_same( c1, c2 ) );
    }
    THEN( "links with same predicate" )
    {
        auto const ca1 = view2::child( "alpha" );
        auto const ca2 = view2::child( "alpha" );
        auto const cb1 = view2::child( "beta" );

        REQUIRE( is_same( ca1, ca1 ) );
        REQUIRE( is_same( ca1, ca2 ) );
        REQUIRE( !is_same( ca1, cb1 ) );
        REQUIRE( !is_same( ca2, cb1 ) );
    }
    THEN( "link with prev link" )
    {
        auto const c1 = view2::child;
        auto const ca1 = view2::child( "alpha" );
        auto const cc1 = view2::child | view2::child;
        auto const cc2 = view2::child | view2::child;
        auto const cac1 = view2::child( "alpha" ) | view2::child;
        auto const caca1 = view2::child( "alpha" ) | view2::child( "alpha" );
        auto const caca2 = view2::child( "alpha" ) | view2::child( "alpha" );

        REQUIRE( is_same( cc1, cc1 ) );
        REQUIRE( is_same( cc1, cc2 ) );
        REQUIRE( !is_same( cc1, c1 ) );
        REQUIRE( !is_same( cc1, ca1 ) );
        REQUIRE( !is_same( cc1, cac1 ) );
        REQUIRE( is_same( caca1, caca1 ) );
        REQUIRE( is_same( caca1, caca2 ) );
        REQUIRE( !is_same( cac1, caca1 ) );
        // REQUIRE( !is_same( cv1, cc1 ) );
        // REQUIRE( !is_same( cv2, cc1 ) );
    }
}

SCENARIO( "view::operator|Link Link", "[node_view][link]" )
{
    GIVEN( "c: view::child, d: view::desc" )
    {
        auto const rt = view2::child | view2::desc;

        THEN( "c | d: view::desc is result type" )
        {
            static_assert( std::is_same_v< decltype( rt ), decltype( view2::desc ) > );
        }
        THEN( "c | d: prev is view::child type, prev has no prev" )
        {
            REQUIRE( dynamic_cast< decltype( view2::child )* >( rt.prev().get() ) != nullptr );
            REQUIRE( rt.prev()->prev().get() == nullptr );
        }
    }
    GIVEN( "c: view::child( 'c' ), d: view::desc( 'd' )" )
    {
        auto const rt = view2::child( "c" ) | view2::desc( "d" );

        THEN( "c | d: view::desc( '1' ) is result type" )
        {
            static_assert( std::is_same_v< decltype( rt ), decltype( view2::desc ) > );
            REQUIRE( rt.pred().has_value() );
            REQUIRE( std::get< char const* >( rt.pred().value() ) == std::string{ "d" } );
        }
        THEN( "c | d: prev is view::child( 'c' ) type, prev has no prev" )
        {
            REQUIRE( rt.pred().has_value() );
            REQUIRE( std::get< char const* >( rt.pred().value() ) == std::string{ "d" } );

            auto const prev = dynamic_cast< decltype( view2::child )* >( rt.prev().get() );

            REQUIRE( prev != nullptr );
            REQUIRE( prev->prev().get() == nullptr );

            REQUIRE( prev->pred().has_value() );
            REQUIRE( std::get< char const* >( prev->pred().value() ) == std::string{ "c" } );
        }
    }
}

} // namespace kmap::view2
