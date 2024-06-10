/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_FUZZY_SEARCH_FUZZY_SEARCH_HPP
#define KMAP_UTIL_FUZZY_SEARCH_FUZZY_SEARCH_HPP

#include <kmap_fwd.hpp>
#include <common.hpp>

#include <string>
#include <vector>

namespace kmap::util {

auto fuzzy_search( std::vector< std::string > const& candidates
                 , std::string const& query
                 , unsigned const& limit )
    -> std::vector< std::string >;
auto fuzzy_search_titles( Kmap const& km
                        , std::string const& query
                        , unsigned const& limit )
    -> std::vector< std::pair< Uuid, std::string > >;

} // namespace kmap::util

#endif // KMAP_UTIL_FUZZY_SEARCH_FUZZY_SEARCH_HPP