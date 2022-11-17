/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_CLERK_HPP
#define KMAP_UTIL_CLERK_HPP

#include "common.hpp"
#include "path/node_view.hpp"

#include <string>

namespace kmap {
class Kmap;
}

namespace kmap::util {

auto confirm_reinstall( std::string const& item
                      , std::string const& path )
    -> Result< bool >;
auto match_body_code( Kmap const& km
                    , view::Intermediary const& vnode
                    , std::string const& content )
    -> bool;
auto match_raw_body( Kmap const& km
                   , view::Intermediary const& vnode
                   , std::string const& content )
    -> bool;

} // namespace kmap::util

#endif // KMAP_UTIL_CLERK_HPP
