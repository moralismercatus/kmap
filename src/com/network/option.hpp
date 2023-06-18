/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_NETWORK_OPTION_HPP
#define KMAP_NETWORK_OPTION_HPP

#include "com/option/option_clerk.hpp"
#include "common.hpp"
#include "component.hpp"
#include "utility.hpp"

namespace kmap::com {

class NetworkOption : public Component
{
    OptionClerk oclerk_;

public:
    static constexpr auto id = "network.option";
    constexpr auto name() const -> std::string_view override { return id; }

    NetworkOption( Kmap& kmap
                  , std::set< std::string > const& requisites
                  , std::string const& description );
    NetworkOption( NetworkOption const& ) = delete;
    NetworkOption( NetworkOption&& ) = default;
    virtual ~NetworkOption() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_standard_options()
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_NETWORK_OPTION_HPP
