/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_LOG_XML_HPP
#define KMAP_UTIL_LOG_XML_HPP

#if KMAP_LOG

#include <common.hpp>

#include <boost/property_tree/ptree.hpp>

namespace kmap::util::log {

auto to_xml( char const* x )
    -> boost::property_tree::ptree;
auto to_xml( std::string const& x )
    -> boost::property_tree::ptree;
auto to_xml( Uuid const& x )
    -> boost::property_tree::ptree;

template< typename T >
auto to_xml( T const& x )
    -> boost::property_tree::ptree
{
    auto n = boost::property_tree::ptree{};

    n.put( "", "n/a" );

    return n;
}

} // namespace kmap::util::log

#endif // KMAP_LOG

#endif // KMAP_UTIL_LOG_XML_HPP