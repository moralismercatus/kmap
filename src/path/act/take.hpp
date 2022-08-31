/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_TAKE_HPP
#define KMAP_PATH_ACT_TAKE_HPP

#include "common.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "util/concepts.hpp"
#include "utility.hpp"

#include <range/v3/view/take.hpp>
#include <range/v3/range/conversion.hpp>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct Take 
{
    uint32_t count = {};

    Take( uint32_t const& cnt );
};

auto take( uint32_t const& count )
    -> Take;

// TODO: Is there any real difference between this and ranges::actions::take? This makes a copy, whereas copies with ranges version must be explictly copied, but is that desirable? Probably not...
//       Of course, there is a valid need for Result< RT >.
template< concepts::Range RT >
auto operator|( RT const& range, Take const& op )
    -> RT
{
    auto rv = RT{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( op.count > 0 )
            {
                BC_ASSERT( rv.size() <= range.size() );
            }
        })
    ;

    rv = range
       | ranges::views::take( op.count )
       | ranges::to< RT >();

    return rv;
}

template< concepts::Range RT >
auto operator|( Result< RT > const& rrange, Take const& op )
    -> Result< RT >
{
    auto rv = KMAP_MAKE_RESULT( RT );

    KMAP_ENSURE( rrange, error_code::common::uncategorized );

    rv = rrange.value() | op;

    return rv;
}

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_ACT_TAKE_HPP
