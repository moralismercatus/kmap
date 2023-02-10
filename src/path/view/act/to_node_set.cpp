/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/to_node_set.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/to_fetch_set.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

#include <deque>

namespace rvs = ranges::views;

namespace kmap::view2::act {

auto to_node_set( Kmap const& km )
    -> ToNodeSet 
{
    return ToNodeSet{ km };
}

auto operator|( Tether const& lhs
              , ToNodeSet const& rhs )
    -> UuidSet
{
    auto rv = UuidSet{};
    // auto const db = KTRYE( rhs.km.fetch_component< com::Database >() );
    // // TODO: qcaching should happen with to_fetch_set, rather.
    // auto& qcache = db->query_cache();

    // if( auto const qr = qcache.fetch( lhs )
    //   ; qr )
    // {
    //     rv = qr.value();
    // }
    // else
    {
        auto const ctx = FetchContext{ rhs.km, lhs };
        auto const ns = lhs | to_fetch_set( ctx );

        rv = ns
           | rvs::transform( []( auto const& e ){ return e.id; } )
           | ranges::to< UuidSet >();

        // KTRYE( const_cast< db::QueryCache& >( qcache ).push( lhs, rv ) ); // TODO: WARNING FLAGS!!! VERY TEMPORARY! const_cast a no-no!
        // KMAP_ENSURE_EXCEPT( qcache.fetch( lhs ) );
    }

    return rv;
}

} // namespace kmap::view2::act
