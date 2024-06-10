/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_ACT_SINGLE_HPP
#define KMAP_PATH_NODE_VIEW2_ACT_SINGLE_HPP

#include <common.hpp>
#include <error/network.hpp>
#include <kmap_fwd.hpp>
#include <path/view/act/actor.hpp>
#include <path/view/tether.hpp>
#include <util/concepts.hpp>
#include <util/result.hpp>

namespace kmap::view2::act {

struct Single : public Actor {};

template< kmap::concepts::Range RT >
auto operator|( Result< RT > const& result, Single const& tso )
    -> Result< typename RT::value_type >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( typename RT::value_type );

    if( result )
    {
        auto const& value = result.value();

        if( value.size() == 1 )
        {
            using std::begin;

            rv = *begin( value );
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::network::ambiguous_path );
        }
    }
    else
    {
        rv = KMAP_PROPAGATE_FAILURE( result );
    }

    return rv;
}

auto const single = Single{};

} // namespace kmap::view2::act

#endif // KMAP_PATH_NODE_VIEW2_ACT_SINGLE_HPP
