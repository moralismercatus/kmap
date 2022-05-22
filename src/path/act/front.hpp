/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_FRONT_HPP
#define KMAP_PATH_ACT_FRONT_HPP

#include "utility.hpp"
#include "util/concepts.hpp"


namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct Front
{
    Front() = default;
};

auto const front = Front{};

template< concepts::Range RT >
auto operator|( RT const& range, Front const& op )
    -> Result< typename RT::value_type >
{
    auto rv = KMAP_MAKE_RESULT( typename RT::value_type );

    if( !range.empty() )
    {
        rv = range.front();
    }

    return rv;
}

// TODO: Provide Result<> version that just calls non-Result version (try/catch => Result)?
// template< concepts::Range RT >
// auto operator|( Result< RT > const& range, Front const& op )
//     -> Result< UuidVec >;
// TODO: Likewise, could also provide a std::set => act::order => std::vector if I make a !concepts::SequenceRange (vector => SequenceContainer) overload.

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_ACT_FRONT_HPP
