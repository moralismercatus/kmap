/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_OPTION_OPTION_HPP
#define KMAP_OPTION_OPTION_HPP

#include "common.hpp"
#include "kmap.hpp"

#include <string_view>

namespace kmap {

class OptionStore
{
    Kmap& kmap_;

public:
    OptionStore( Kmap& kmap );

    auto apply( Uuid const& option )
        -> Result< void >;
    auto apply( std::string const& path )
        -> Result< void >;
    auto apply_all()
        -> Result< void >;
    auto option_root()
        -> Uuid;
    auto install_option( Heading const& heading
                       , std::string const& descr
                       , std::string const& value 
                       , std::string const& action )
        -> Result< Uuid >;
    auto is_option( Uuid const& node )
        -> bool;
    auto uninstall_option( Heading const& heading )
        -> Result< void >;
    auto uninstall_option( Uuid const& option )
        -> Result< void >;
    auto update_value( Heading const& heading
                     , std::string const& value )
        -> Result< void >;
};

} // namespace kmap

#endif // KMAP_OPTION_OPTION_HPP
