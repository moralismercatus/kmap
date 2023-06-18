/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/exists.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/to_node_set.hpp"
#include <com/network/network.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/all_of.hpp>

namespace kmap::view2::act {

auto exists( Kmap const& km )
    -> Exists
{
    return Exists{ km };
}

auto operator|( Tether const& lhs
              , Exists const& rhs )
    -> bool
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( rhs.km.fetch_component< com::Network >() );
    auto const nodes = lhs | to_node_set( rhs.km );

    // Note: Cannot just rely on nodes.empty(), as an anchor can be supplied as a Tether (example: `anchor::node( n1 ) | act2::exists( km );`)
    //       So must check all results for existence.
    // Note: `all_of` returns true if given empty range, so must check this first.
    return !nodes.empty() && ranges::all_of( nodes, [ &nw ]( auto const& n ){ return nw->exists( n ); } );
}

} // namespace kmap::view2::act
