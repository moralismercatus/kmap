/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_AUTOSAVE_HPP
#define KMAP_DB_AUTOSAVE_HPP

#include "common.hpp"
#include "component.hpp"

#include <cstdint>
#include <string>
#include <set>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class Autosave : public Component
{
    uint64_t counter_ = {};
    uint64_t threshold_ = {};

public:
    static constexpr auto id = "autosave";
    constexpr auto name() const -> std::string_view override { return id; }

    Autosave( Kmap& kmap
            , std::set< std::string > const& requisites
            , std::string const& description );
    virtual ~Autosave() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto has_event_outlet( std::set< std::string > const& requisites )
        -> bool;
    auto install_event_outlet( std::string const& unit )
        -> Result< void >;
    auto uninstall_event_outlet()
        -> Result< void >;
    auto interval()
        -> Result< void >;
    auto set_threshold( uint64_t const threshold )
        -> void;
};

} // namespace kmap::com

#endif // KMAP_DB_AUTOSAVE_HPP 
