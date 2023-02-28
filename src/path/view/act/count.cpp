/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/count.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/to_node_set.hpp"
#include "path/view/act/to_string.hpp" // Testing

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2::act {

auto count( Kmap const& km )
    -> Count
{
    return Count{ km };
}

auto operator|( Tether const& lhs
              , Count const& rhs )
    -> unsigned
{
    return ( lhs | act::to_node_set( rhs.km ) ).size();
}

} // namespace kmap::view2::act
