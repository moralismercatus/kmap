/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "tether.hpp"

#include "path/view/act/to_string.hpp"
#include "path/view/anchor/anchor.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2 {

Tether::Tether( Anchor::AnchorPtr anc )
    : anchor{ std::move( anc ) }
{
}

Tether::Tether( Anchor::AnchorPtr anc
              , Link::LinkPtr tlink )
    : anchor{ std::move( anc ) }
    , tail_link{ std::move( tlink ) }
{
}

auto Tether::operator<( Tether const& other ) const
    -> bool
{
    if( ( *anchor ) < ( *other.anchor ) )
    {
        return true;
    }
    else if( ( *other.anchor ) < ( *anchor ) )
    {
        return false;
    }
    else if( !tail_link || !other.tail_link )
    {
        return tail_link < other.tail_link;
    }
    else
    {
        return ( *tail_link ) < ( *other.tail_link );
    }
}

SCENARIO( "Tether::operator<", "[node_view][link]")
{
    // TODO: Impl. test.
    // GIVEN( "anchor::node( id ) | view2::all_of( view2::direct_desc, { <str>, <str>, <str> } )" )
    // {
    //     auto const acn = gen_uuid();
    //     auto const t1_ct = anchor::node( acn )
    //                      | view2::all_of( view2::direct_desc
    //                                     , { "subject.sierra", "verb.victor", "object.oscar" } );
    //     auto const t1 = t1_ct | to_tether;
    //     auto const t2 = t1_ct | to_tether;
    //     auto const t3_ct = anchor::node( acn )
    //                      | view2::all_of( view2::direct_desc
    //                                     , { "subject.sierra", "verb.victor", "object.oscar" } );
    //     auto const t3 = t3_ct | to_tether;
        
    //     REQUIRE( !( t1 < t1 ) );
    //     REQUIRE( !( t2 < t2 ) );
    //     REQUIRE( !( t3 < t3 ) );
    //     REQUIRE(( !( t1 < t2 ) && !( t2 < t1 ) ));
    //     REQUIRE(( !( t1 < t3 ) && !( t3 < t1 ) ));
    // }
}

auto Tether::to_string() const
    -> std::string
{
    if( tail_link )
    {
        return fmt::format( "Tether|anchor:{}|chain:{}\n"
                          , *anchor | act::to_string
                          , *tail_link | act::to_string );
    }
    else
    {
        return fmt::format( "Tether|anchor:{}\n"
                          , *anchor | act::to_string );
    }
}

auto operator<<( std::ostream& os
               , Tether const& tether )
    -> std::ostream&
{
    os << ( tether | act::to_string );

    return os;
}

} // namespace kmap::view2
