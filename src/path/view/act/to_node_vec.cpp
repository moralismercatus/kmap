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

auto try_node_vec( Kmap const& km )
    -> TryNodeVec 
{
    return TryNodeVec{ km };
}

auto operator|( Tether const& lhs
              , ToNodeVec const& rhs )
    -> UuidVec
{
    KM_RESULT_PROLOG();

    return KTRYE( lhs | try_node_vec( rhs.km ) );
}

auto operator|( Tether const& lhs
              , TryNodeVec const& rhs )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< UuidVec >();
    auto const ctx = FetchContext{ rhs.km, lhs };
    auto const fs = KTRY( lhs | to_fetch_set( ctx ) );
    auto const& fsv = fs.get< random_access_index >();

    rv = fsv
        | rvs::transform( []( auto const& e ){ return e.id; } )
        | ranges::to< UuidVec >();

    return rv;
}

} // namespace kmap::view2::act
