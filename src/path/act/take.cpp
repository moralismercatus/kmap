/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "take.hpp"

#include "test/util.hpp" 
#include "path/act/order.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view::act {

Take::Take( uint32_t const& cnt )
    : count{ cnt }
{
}

auto take( uint32_t const& count )
    -> Take
{
    return Take{ count };
}

// Test goes here...
// TODO: Q: what if { 1, 2, 3 } | take( 5 ) => ? Should it err or return { 1, 2, 3 }? I think ranges::views::take returns { 1, 2, 3 }
SCENARIO( "fetch ancestral lineage up to count", "[path][node_view][take]" )
{
    using test::headings;
    using namespace std::string_literals;

    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    GIVEN( "lineage of depth of 3" )
    {
        auto& km = kmap::Singleton::instance();
        auto const root = km.root_node_id();
        auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );

        REQUIRE_TRY( view::make( root )
                   | view::direct_desc( "1.2.3" )
                   | view::create_node( km ) );
        
        auto const vn3 = view::make( root ) | view::direct_desc( "1.2.3" ) | view::lineage;

        THEN( "{ /, 1, 2, 3 } | take( 3 ) => { /, 1, 2, }" )
        {
            auto const r = vn3
                         | view::to_node_set( km )
                         | act::order( km )
                         | act::take( 3 );
            
            REQUIRE( headings( *nw, r ) == std::vector{ "root"s, "1"s, "2"s } );
        }
        THEN( "take( size ) where size == range.size() => size" )
        {
            auto const r = vn3
                         | view::to_node_set( km )
                         | act::order( km )
                         | act::take( 4 );
            
            REQUIRE( headings( *nw, r ) == std::vector{ "root"s, "1"s, "2"s, "3"s } );
        }
        THEN( "take( size ) where size < range.size() => size" )
        {
            auto const r = vn3
                         | view::to_node_set( km )
                         | act::order( km )
                         | act::take( 1 );
            
            REQUIRE( headings( *nw, r ) == std::vector{ "root"s } );
        }
        THEN( "take( size ) where size > range.size() => range.size()" )
        {
            auto const r = vn3
                         | view::to_node_set( km )
                         | act::order( km )
                         | act::take( 6 );
            
            REQUIRE( headings( *nw, r ) == std::vector{ "root"s, "1"s, "2"s, "3"s } );
        }
        THEN( "take( size ) where size == 0 => {}" )
        {
            auto const r = vn3
                         | view::to_node_set( km )
                         | act::order( km )
                         | act::take( 0 );
            
            REQUIRE( headings( *nw, r ) == std::vector< std::string >{} );
        }
    }

}

} // namespace kmap::view::act