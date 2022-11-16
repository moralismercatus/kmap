/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_OPTION_OPTION_HPP
#define KMAP_OPTION_OPTION_HPP

#include "common.hpp"
#include "component.hpp"

#include <string_view>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class OptionStore : public Component
{
public:
    static constexpr auto id = "option_store";
    constexpr auto name() const -> std::string_view override { return id; }

    OptionStore( Kmap& kmap
               , std::set< std::string > const& requisites
               , std::string const& description );
    virtual ~OptionStore() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

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

} // namespace kmap::com

#endif // KMAP_OPTION_OPTION_HPP