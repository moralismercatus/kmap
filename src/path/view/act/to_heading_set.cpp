/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/to_heading_set.hpp"


#include "com/network/network.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/to_node_set.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::view2::act {

auto to_heading_set( Kmap const& km )
    -> ToHeadingSet
{
    return ToHeadingSet{ km };
}

auto operator|( Tether const& lhs
              , ToHeadingSet const& rhs )
    -> std::set< std::string > 
{
    KM_RESULT_PROLOG();

    auto rv = std::set< std::string >{};
    auto const ns = lhs | act::to_node_set( rhs.km );

    if( !ns.empty() )
    {
        auto const nw = KTRYE( rhs.km.fetch_component< com::Network >() );

        rv = ns
           | rvs::transform( [ & ]( auto const& n ){ return KTRYE( nw->fetch_heading( n ) ); })
           | ranges::to< std::set >();
    }

    return rv;
}

} // namespace kmap::view2::act
