/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_AUTOSAVE_HPP
#define KMAP_DB_AUTOSAVE_HPP

#include "common.hpp"

#include <cstdint>

namespace kmap
{
    class Kmap;
}

namespace kmap::db {

class Autosave
{
    Kmap& kmap_;
    uint64_t counter_ = {};
    uint64_t threshold_ = {};

public:
    Autosave( Kmap& kmap );

    auto initialize()
        -> Result< void >;
    auto install_event_outlet( std::string const& unit )
        -> Result< void >;
    auto uninstall_event_outlet()
        -> Result< void >;
    auto interval()
        -> Result< void >;
    auto set_threshold( uint64_t const threshold )
        -> void;
};

} // namespace kmap::db

#endif // KMAP_DB_AUTOSAVE_HPP 
