/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_FILESYSTEM_HPP
#define KMAP_FILESYSTEM_HPP

#include "common.hpp"
#include "component.hpp"

#include <fstream>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

#if !KMAP_NATIVE
auto const kmap_root_dir = FsPath{ "/kmap" }; // TODO: Don't make this publicly available. Make clients require the component top access, ensuring no files are used until this is initialized.
#else
auto const kmap_root_dir = FsPath{ "./" }; // TODO: Don't make this publicly available. Make clients require the component top access, ensuring no files are used until this is initialized.
#endif // !KMAP_NATIVE

class Filesystem : public Component
{
public:
    static constexpr auto id = "filesystem";
    constexpr auto name() const -> std::string_view override { return id; }

    using Component::Component;
    virtual ~Filesystem();

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto init_ems_nodefs()
        -> void;
    auto install_standard_commands()
        -> Result< void >;

    auto open_ifstream( std::string const& path )
        -> std::ifstream;
    auto open_ofstream( std::string const& path )
        -> std::ofstream;
};

auto complete_filesystem_path( std::string const& raw )
    -> StringVec;

} // namespace kmap::com

#endif // KMAP_FILESYSTEM_HPP
