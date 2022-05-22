/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_PUSH_HPP
#define KMAP_PATH_ACT_PUSH_HPP

#include "utility.hpp"
#include "util/concepts.hpp"


namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

template< typename T >
struct Push
{
    T const& item;

    Push( T const& t ) : item{ t } {}
};

template< typename T >
auto push( T const& t )
    -> Push< T >
{
    return Push< T >{ t };
}

template< concepts::Range RT >
auto operator|( RT&& range, Push< typename RT::value_type > const& op )
    -> RT
{
    range.emplace( op.item );

    return range;
}

// TODO: Provide Result<> version that just calls non-Result version (try/catch => Result)?
// template< concepts::Range RT >
// auto operator|( Result< RT > const& range, Push const& op )
//     -> Result< UuidVec >;
// TODO: Likewise, could also provide a std::set => act::order => std::vector if I make a !concepts::SequenceRange (vector => SequenceContainer) overload.

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_ACT_PUSH_HPP
