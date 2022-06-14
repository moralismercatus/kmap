/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_ORDER_HPP
#define KMAP_PATH_ACT_ORDER_HPP

#include "utility.hpp"
#include "util/concepts.hpp"

#include <range/v3/algorithm/sort.hpp>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct Order
{
    Kmap const& kmap;

    Order( Kmap const& km ) : kmap{ km } {}
};

auto order( Kmap const& kmap )
    -> Order;

template< concepts::Range RT >
auto operator|( RT const& range, Order const& op )
    -> UuidVec
{
    auto const sfn = [ &op ]( auto const& lhs, auto const& rhs )
    {
        return is_ordered( op.kmap, lhs, rhs );
    };
    auto tr = UuidVec{ range.begin(), range.end() };

    ranges::sort( tr, sfn );

    return tr;
}

// TODO: Provide Result<> version that just calls non-Result version (try/catch => Result)?
// template< concepts::Range RT >
// auto operator|( Result< RT > const& range, Order const& op )
//     -> Result< UuidVec >;
// TODO: Likewise, could also provide a std::set => act::order => std::vector if I make a !concepts::SequenceRange (vector => SequenceContainer) overload.

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_ACT_ORDER_HPP
