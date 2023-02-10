/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/none_of.hpp"

#include "common.hpp"
#include "path/view/anchor/node.hpp"
#include "path/view/common.hpp"
#include "path/view/direct_desc.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/set_algorithm.hpp>

#include <vector>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto NoneOf::create( CreateContext const& ctx
                   , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< UuidSet >();

    rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "NoneOf::create is probably not what you want" );

    return rv;
}

auto NoneOf::fetch( FetchContext const& ctx
                  , Uuid const& node ) const
    -> FetchSet
{
    auto rset = FetchSet{};

    for( auto const& ls = links()
       ; auto const& pred_link : ls )
    {
        auto const unpred_link = pred_link->new_link();

        if( auto unpred_fetched = unpred_link->fetch( ctx, node )
                                | ranges::to< std::vector >()
          ; unpred_fetched.size() > 0 )
        {
            ranges::sort( unpred_fetched );

            if( auto pred_fetched = pred_link->fetch( ctx, node )
                                  | ranges::to< std::vector >()
              ; pred_fetched.size() > 0 )
            {
                ranges::sort( pred_fetched );

                rset = rvs::set_intersection( unpred_fetched, pred_fetched )
                     | ranges::to< FetchSet >();
            }
            else
            {
                rset = unpred_fetched
                     | ranges::to< FetchSet >();
            }
        }
    }

    return rset;
}

auto NoneOf::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< NoneOf >();
}

auto NoneOf::to_string() const
    -> std::string
{
    if( auto const& ls = links()
      ; !ls.empty() )
    {
        return "none_of( 'TODO' )";
    }
    else
    {
        return "none_of";
    }
}

auto NoneOf::compare_less( Link const& other ) const
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

// TODO: Missing some impl., uncomment once completed.
// SCENARIO( "NoneOf::compare_less", "[node_view][link]")
// {
//     GIVEN( "anchor::node( id ) | view2::none_of( view2::direct_desc, { <str>, <str>, <str> } )" )
//     {
//         auto const acn = gen_uuid();
//         auto const t1_ct = anchor::node( acn )
//                          | view2::none_of( view2::direct_desc
//                                          , { "subject.sierra", "verb.victor", "object.oscar" } );
//         auto const t1 = t1_ct | to_tether;
//         auto const t2 = t1_ct | to_tether;
//         auto const t3_ct = anchor::node( acn )
//                          | view2::none_of( view2::direct_desc
//                                          , { "subject.sierra", "verb.victor", "object.oscar" } );
//         auto const t3 = t3_ct | to_tether;
        
//         REQUIRE( !( t1 < t1 ) );
//         REQUIRE( !( t2 < t2 ) );
//         REQUIRE( !( t3 < t3 ) );
//         REQUIRE(( !( t1 < t2 ) && !( t2 < t1 ) ));
//         REQUIRE(( !( t1 < t3 ) && !( t3 < t1 ) ));
//     }
// }

} // namespace kmap::view2
