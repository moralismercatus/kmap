/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_NETWORK_COMMAND_HPP
#define KMAP_NETWORK_COMMAND_HPP

#include "com/cmd/cclerk.hpp"
#include "common.hpp"
#include "component.hpp"
#include "utility.hpp"

namespace kmap::com {

class NetworkCommand : public Component
{
    CommandClerk cclerk_;

public:
    static constexpr auto id = "network.command";
    constexpr auto name() const -> std::string_view override { return id; }

    NetworkCommand( Kmap& kmap
                  , std::set< std::string > const& requisites
                  , std::string const& description );
    NetworkCommand( NetworkCommand const& ) = delete;
    NetworkCommand( NetworkCommand&& ) = default;
    virtual ~NetworkCommand() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_standard_commands()
        -> void;
};

} // namespace kmap::com

#endif // KMAP_NETWORK_COMMAND_HPP
