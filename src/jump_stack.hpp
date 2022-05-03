/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_JUMP_STACK_HPP
#define KMAP_JUMP_STACK_HPP

#include "common.hpp"
#include "utility.hpp"

#include <boost/circular_buffer.hpp>

#include <stack>
#include <vector>

namespace kmap
{

// TODO: Disable self-pushing. E.g., if we're at .root, and we select( "/" ), don't push.
class JumpStack
{
public:
    size_t const upper_bound = 256u;

    JumpStack();

    auto jump_in( Uuid const& id )
        -> Uuid;
    auto jump_in()
        -> Optional< Uuid >;
    [[ maybe_unused ]]
    auto jump_out()
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto in_top() const
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto out_top() const
        -> Optional< Uuid >;

    JumpStack( JumpStack const& ) = delete;
    auto operator==( JumpStack const& ) = delete;

private:
    using CircularBuffer = boost::circular_buffer< Uuid >;

    CircularBuffer in_buf_;
    CircularBuffer out_buf_;
};

} // namespace kmap

#endif // KMAP_JUMP_STACK_HPP
