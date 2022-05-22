/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "alias.hpp"

#include "error/master.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>


namespace kmap {

auto make_alias_id( Uuid const& alias_src
                  , Uuid const& alias_dst )
    -> Uuid
{
    return xor_ids( alias_src, alias_dst );
}

} // namespace kmap

using namespace kmap;

SCENARIO( "AliasSet basic ops" )
{
    auto aliases = AliasSet{};

    THEN( "empty set" )
    {
        REQUIRE( aliases.get< AliasItem::alias_type >().empty() );
        REQUIRE( aliases.get< AliasItem::src_type >().empty() );
        REQUIRE( aliases.get< AliasItem::dst_type >().empty() );
    }

    GIVEN( "alias item" )
    {
        auto const src = AliasItem::src_type{ gen_uuid() };
        auto const dst = AliasItem::dst_type{ gen_uuid() };
        auto const alias = make_alias_id( src.value(), dst.value() );

        aliases.emplace( AliasItem{ src, dst } );
                                  
        THEN( "duplicate alias insertion fails" )
        {
            auto const& v = aliases.get< AliasItem::alias_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( !aliases.emplace( AliasItem{ src, dst } ).second );
            REQUIRE( v.size() == 1 );
        }
        THEN( "items exist from alias view" )
        {
            auto const& v = aliases.get< AliasItem::alias_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( alias ) );

            auto const it = v.find( alias ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == src );
            REQUIRE( item.dst() == dst );
        }
        THEN( "items exist from src view" )
        {
            auto const& v = aliases.get< AliasItem::src_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( src ) );

            auto const it = v.find( src ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == src );
            REQUIRE( item.dst() == dst );
        }
        THEN( "items exist from dst view" )
        {
            auto const& v = aliases.get< AliasItem::dst_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( dst ) );

            auto const it = v.find( dst ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == src );
            REQUIRE( item.dst() == dst );
        }
    }
}