/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_SCRIPT_HPP
#define KMAP_UTIL_SCRIPT_HPP

#include "common.hpp"

#include <istream>

namespace kmap {
class Kmap;
}

namespace kmap::util {

auto load_script( Kmap& kmap 
                , std::istream& is )
    -> Result< void >;
auto to_js_body_code( std::string const& raw_code )
    -> std::string;
auto to_kscript_body_code( std::string const& raw_code )
    -> std::string;

} // namespace kmap::util

#endif // KMAP_UTIL_SCRIPT_HPP
