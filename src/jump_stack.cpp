/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "jump_stack.hpp"

#include "contract.hpp"

namespace kmap
{

JumpStack::JumpStack()
    : in_buf_( upper_bound )
    , out_buf_( upper_bound )
{
}

auto JumpStack::jump_in( Uuid const& id )
    -> Uuid
{
    if( auto const top = out_top()
      ; !top
     || *top != id )
    {
        out_buf_.push_back( id );
    }

    return id;
}

auto JumpStack::jump_in()
    -> Optional< Uuid >
{
    if( in_buf_.empty() )
    {
        return nullopt;
    }

    auto const rv = in_buf_.back();

    in_buf_.push_back( rv );
    in_buf_.pop_back();

    return rv;
}

auto JumpStack::jump_out()
    -> Optional< Uuid >
{
    if( out_buf_.empty() )
    {
        return nullopt;
    }

    auto const rv = out_buf_.back();

    in_buf_.push_back( rv );
    out_buf_.pop_back();

    return { rv };
}

auto JumpStack::in_top() const
    -> Optional< Uuid >
{
    if( in_buf_.empty() )
    {
        return nullopt;
    }

    return in_buf_.back();
}

auto JumpStack::out_top() const
    -> Optional< Uuid >
{
    if( out_buf_.empty() )
    {
        return nullopt;
    }

    return out_buf_.back();
}

}
