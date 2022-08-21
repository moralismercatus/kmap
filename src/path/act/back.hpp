/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_BACK_HPP
#define KMAP_PATH_ACT_BACK_HPP

#include "utility.hpp"
#include "util/concepts.hpp"


namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct Back
{
    Back() = default;
};

auto const back = Back{};

template< concepts::Range RT >
auto operator|( RT const& range, Back const& op )
    -> Result< typename RT::value_type >
{
    auto rv = KMAP_MAKE_RESULT( typename RT::value_type );

    if( !range.empty() )
    {
        rv = range.back();
    }

    return rv;
}

template< concepts::Range RT >
auto operator|( kmap::Result< RT > const& rrange, Back const& op )
    -> Result< typename RT::value_type >
{
    auto rv = KMAP_MAKE_RESULT( typename RT::value_type );

    if( rrange )
    {
        auto const& range = rrange.value();

        if( !range.empty() )
        {
            rv = range.back();
        }
    }

    return rv;
}

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_ACT_BACK_HPP
