/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_FS_HPP
#define KMAP_DB_FS_HPP

#include "common.hpp"
#include "component.hpp"
#include "com/cmd/cclerk.hpp"

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

/**
 * @note: It looks clean to store the JS Network instance as a member here, but the trouble is that emscripten lacks support for JS exception => C++, so any JS exception
 *        raised while execiting `js_new_->call()`s will result in an uncaught (read: cryptic) error lacking details. Is there a way aroud this?
 * 
 */
class DatabaseFilesystem : public Component
{
    CommandClerk cclerk_;

public:
    static constexpr auto id = "database.filesystem";
    constexpr auto name() const -> std::string_view override { return id; }

    DatabaseFilesystem( Kmap& kmap
                      , std::set< std::string > const& requisites
                      , std::string const& description );
    virtual ~DatabaseFilesystem() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_standard_commands()
        -> void;

    auto copy_state( FsPath const& dst )
        -> Result< void >;
    auto move_state( FsPath const& dst )
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_DB_FS_HPP
