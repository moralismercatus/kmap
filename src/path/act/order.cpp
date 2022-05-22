/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "order.hpp"

namespace kmap::view::act {

auto order( Kmap const& kmap )
    -> Order
{
    return Order{ kmap };
}

} // namespace kmap::view::act