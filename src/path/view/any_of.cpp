/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/any_of.hpp"

#include "common.hpp"
#include "path/view/act/to_string.hpp"
#include "path/view/anchor/node.hpp"
#include "path/view/child.hpp"
#include "path/view/common.hpp"
#include "path/view/direct_desc.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2 {

auto AnyOf::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< UuidSet >();

    rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "AnyOf::create is probably not what you want" );

    return rv;
}

auto AnyOf::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> FetchSet
{
    auto rset = FetchSet{};

    for( auto const& ls = links()
       ; auto const& link : ls )
    {
        auto const fetched = anchor::node( node ) | link | act::to_fetch_set( ctx );

        rset.insert( fetched.begin(), fetched.end() );
    }

    return rset;
}

auto AnyOf::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< AnyOf >();
}

auto AnyOf::to_string() const
    -> std::string
{
    if( auto const& ls = links()
      ; !ls.empty() )
    {
        return "any_of( 'TODO' )";
    }
    else
    {
        return "any_of";
    }
}

auto AnyOf::compare_less( Link const& other ) const
    -> bool
{
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return links() < dynamic_cast< decltype( *this )& >( other ).links();
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

SCENARIO( "AnyOf::compare_less", "[node_view][link]")
{
    GIVEN( "anchor::node( id ) | view2::any_of( view2::direct_desc, { <str>, <str>, <str> } )" )
    {
        auto const acn = gen_uuid();
        auto const t1_ct = anchor::node( acn )
                         | view2::any_of( view2::direct_desc
                                        , { "subject.sierra", "verb.victor", "object.oscar" } );
        auto const t1 = t1_ct | to_tether;
        auto const t2 = t1_ct | to_tether;
        auto const t3_ct = anchor::node( acn )
                         | view2::any_of( view2::direct_desc
                                        , { "subject.sierra", "verb.victor", "object.oscar" } );
        auto const t3 = t3_ct | to_tether;
        
        REQUIRE( !( t1 < t1 ) );
        REQUIRE( !( t2 < t2 ) );
        REQUIRE( !( t3 < t3 ) );
        REQUIRE(( !( t1 < t2 ) && !( t2 < t1 ) ));
        REQUIRE(( !( t1 < t3 ) && !( t3 < t1 ) ));
    }
}

} // namespace kmap::view2
