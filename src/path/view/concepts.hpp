/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_CONCEPTS_HPP
#define KMAP_PATH_NODE_VIEW2_CONCEPTS_HPP

#include <concepts>

namespace kmap::view2 {
    class Link;
    class Tether;
}

namespace kmap::view2::concepts {

template< typename... Args >
concept LinkArgs = ( std::derived_from< Args, kmap::view2::Link > && ... );
template< typename... Args >
concept TetherArgs = ( std::derived_from< Args, kmap::view2::Tether > && ... );

} // namespace kmap::view2::concepts

#endif // KMAP_PATH_NODE_VIEW2_CONCEPTS_HPP
