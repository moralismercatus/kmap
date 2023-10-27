/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_DISAMBIGUATE
#define KMAP_PATH_DISAMBIGUATE

#include <common.hpp>
#include <kmap_fwd.hpp>

#include <map>
#include <string>

namespace kmap {

auto disambiguate_path3( Kmap const& km
                       , std::string const& heading )
    -> Result< std::map< Uuid, std::string > >;
auto disambiguate_path3( Kmap const& km
                       , Uuid const& node )
    -> Result< std::map< Uuid, std::string > >;

} // namespace kmap

#endif // KMAP_PATH_DISAMBIGUATE