/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/ancestry.hpp>

#include <com/network/network.hpp>
#include <contract.hpp>
#include <error/result.hpp>
#include <kmap.hpp>
#include <util/concepts.hpp>
#include <utility.hpp>

#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::path {

auto fetch_ancestry_ordered( Kmap const& km
                           , Uuid const node )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto nv = UuidVec{};
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto it_node = nw->fetch_parent( node );

    while( it_node )
    {
        auto const cn = it_node.value();
        nv.emplace_back( cn );

        it_node = nw->fetch_parent( cn );
    }

    KTRY_UNLESS( it_node, error_code::network::invalid_parent );

    rv = nv | rvs::reverse | ranges::to< UuidVec >();

    return rv;
}

} // namespace kmap::path
