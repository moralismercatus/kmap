
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/node_view2.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2 {

SCENARIO( "view::operator|TetherCT Link", "[node_view][link]" )
{
    GIVEN( "TetherCT<abs_root, direct_desc(pred)>" )
    {
        auto const outlet_root = anchor::abs_root | view2::direct_desc( "meta.event.outlet" );
        auto const outlet = view2::child( "requisite" );
        auto const requisite = view2::child( "requisite" ) | view2::alias;

        THEN( "outlet_root checks out" )
        {
            REQUIRE( outlet_root.tail_link.prev().get() == nullptr );
            REQUIRE( outlet_root.tail_link.pred().has_value()  );
            REQUIRE( std::get< char const* >( outlet_root.tail_link.pred().value() ) == std::string{ "meta.event.outlet" } );
        }

        GIVEN( "outlet_root | view2::desc( outlet )" )
        {
            auto const tv = outlet_root | view2::desc( outlet ); // => abs_root | direct_desc( "meta.event.outlet" ) | desc( child( "requisite" ) )

            THEN( "properties check out" )
            {
                REQUIRE( tv.tail_link.pred().has_value() );
                REQUIRE( tv.tail_link.prev() );
                auto const& p1 = *tv.tail_link.prev();
                REQUIRE( dynamic_cast< view2::DirectDesc const* >( &p1 ) != nullptr );
                auto const& p1c = dynamic_cast< view2::DirectDesc const& >( p1 );
                REQUIRE( p1c.pred().has_value() );
            }
        }
        GIVEN( "outlet_root | view2::desc( outlet ) | requisite" )
        {
            auto const tv = outlet_root
                          | view2::desc( outlet )
                          | requisite;
            fmt::print( "failing: {}\n", tv | act2::to_string );

            THEN( "properties check out" )
            {
                REQUIRE( !tv.tail_link.pred().has_value() );
                REQUIRE( tv.tail_link.prev() );
                auto const& p1 = *tv.tail_link.prev();
                REQUIRE( dynamic_cast< view2::Child const* >( &p1 ) != nullptr );
                auto const& p1c = dynamic_cast< view2::Child const& >( p1 );
                REQUIRE( p1c.pred().has_value() );
                REQUIRE( p1c.prev() );
                auto const& p2 = *p1c.prev();
                REQUIRE( dynamic_cast< view2::Desc const* >( &p2 ) != nullptr );
                auto const& p2c = dynamic_cast< view2::Desc const& >( p2 );
                REQUIRE( p2c.pred().has_value() );
                REQUIRE( p2c.prev() );
                // TODO: Check for each prev() to make sure they meet expectations.
                // REQUIRE( std::get< char const* >( tv.tail_link.pred().value() ) == outlet );
            }
        }
    }
}

} // namespace kmap::view
