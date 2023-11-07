/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_MARKDOWN_HPP
#define KMAP_UTIL_MARKDOWN_HPP

#include <string>

namespace kmap {

auto markdown_to_html( std::string const& text )
    -> std::string;

} // namespace kmap

#endif // KMAP_UTIL_MARKDOWN_HPP