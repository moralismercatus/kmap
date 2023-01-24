/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/exists.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/to_node_set.hpp"

#include <catch2/catch_test_macros.hpp>

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
    return !( ( lhs | act::to_node_set( rhs.km ) ).empty() );
}

} // namespace kmap::view2::act
