/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_WINDOW_HPP
#define KMAP_UTIL_WINDOW_HPP

#include "common.hpp"

#include <cstdint>

namespace kmap::window {

auto inner_width()
    -> uint32_t;
auto inner_height()
    -> uint32_t;
auto set_default_window_title()
    -> Result< void >;

} // namespace kmap::window

#endif // KMAP_UTIL_WINDOW_HPP