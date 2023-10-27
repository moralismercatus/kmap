/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/act/to_node_vec.hpp>

#include <contract.hpp>
#include <kmap.hpp>
#include <path/view/act/to_fetch_set.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

namespace rvs = ranges::views;

namespace kmap::view2::act {

auto to_node_vec( Kmap const& km )
    -> ToNodeVec 
{
    return ToNodeVec{ km };
}

auto operator|( Tether const& lhs
              , ToNodeVec const& rhs )
    -> UuidVec
{
    KM_RESULT_PROLOG();

    auto rv = UuidVec{};
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
        auto const fs = KTRYE( lhs | to_fetch_set( ctx ) );
        auto const& fsv = fs.get< random_access_index >();

        rv = fsv
           | rvs::transform( []( auto const& e ){ return e.id; } )
           | ranges::to< UuidVec >();

        // KTRYE( const_cast< db::QueryCache& >( qcache ).push( lhs, rv ) ); // TODO: WARNING FLAGS!!! VERY TEMPORARY! const_cast a no-no!
        // KMAP_ENSURE_EXCEPT( qcache.fetch( lhs ) );
    }

    return rv;
}

} // namespace kmap::view2::act
