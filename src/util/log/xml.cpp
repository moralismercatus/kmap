/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#if KMAP_LOG

#include <util/log/xml.hpp>

#include <com/network/network.hpp>
#include <common.hpp>
#include <contract.hpp>
#include <kmap.hpp>
#include <path.hpp>

#include <boost/property_tree/ptree.hpp>

#include <iostream>

namespace kmap::util::log {

auto to_xml( char const* x )
    -> boost::property_tree::ptree
{
    auto n = boost::property_tree::ptree{};

    n.put( "", x );

    return n;
}

auto to_xml( std::string const& x )
    -> boost::property_tree::ptree
{
    auto n = boost::property_tree::ptree{};

    n.put( "", x );

    return n;
}

auto to_xml( Uuid const& x )
    -> boost::property_tree::ptree
{
    auto n = boost::property_tree::ptree{};
    auto const& km = kmap::Singleton::instance();

    n.put( "uuid", to_string( x ) );

    if( auto const ap = absolute_path_flat( km, x )
      ; ap )
    {
        n.put( "path", ap.value() );
    }
    else if( auto nw = kmap::Singleton::instance().fetch_component< com::Network >()
      ; nw )
    {
        if( auto const heading = nw.value()->fetch_heading( x )
          ; heading )
        {
            n.put( "heading", heading.value() );
        }
    }

    return n;
}

} // namespace kmap::util::log

#endif // KMAP_LOG
