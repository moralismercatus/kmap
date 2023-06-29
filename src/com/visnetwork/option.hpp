/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_VISNETWORK_OPTION_HPP
#define KMAP_VISNETWORK_OPTION_HPP

#include <com/option/option_clerk.hpp>
#include <common.hpp>
#include <component.hpp>
#include <utility.hpp>

namespace kmap::com {

class VisnetworkOption : public Component
{
    OptionClerk oclerk_;

public:
    static constexpr auto id = "visnetwork.option";
    constexpr auto name() const -> std::string_view override { return id; }

    VisnetworkOption( Kmap& kmap
                    , std::set< std::string > const& requisites
                    , std::string const& description );
    VisnetworkOption( VisnetworkOption const& ) = delete;
    VisnetworkOption( VisnetworkOption&& ) = default;
    virtual ~VisnetworkOption() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_standard_options()
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_VISNETWORK_OPTION_HPP
