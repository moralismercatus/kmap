/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_JSON_HPP
#define KMAP_UTIL_JSON_HPP

#include <common.hpp>

#include <boost/json.hpp>

namespace kmap {

auto fetch_array( boost::json::object const& obj
                , std::string const& key )
    -> Result< boost::json::array >;
auto fetch_bool( boost::json::object const& obj
               , std::string const& key )
    -> Result< bool >;
auto fetch_float( boost::json::object const& obj
                , std::string const& key )
    -> Result< double >;
auto fetch_string( boost::json::object const& obj
                 , std::string const& key )
    -> Result< std::string >;
auto fetch_value( boost::json::object const& obj
                , std::string const& key )
    -> Result< boost::json::value >;

} // namespace kmap

#endif // KMAP_UTIL_JSON_HPP