/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COM_COMMAND_HPP
#define KMAP_COM_COMMAND_HPP

#include <common.hpp>
#include <component.hpp>
#include <path/node_view2.hpp>

#include <set>
#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

struct Argument
{
    std::string path;
    std::string description;
    std::string guard;
    std::string completion;
};

struct Command
{
    struct Argument
    {
        Heading heading;
        std::string description;
        Heading argument_alias;
    };

    std::string path;
    std::string description;
    std::vector< Argument > arguments;
    std::string guard; // TODO: Why not multiple guards? Just aliases to check...
    std::string action;
};

struct Guard
{
    std::string path;
    std::string description;
    std::string action;
};

class CommandStore : public Component
{
public:
    static constexpr auto id = "command_store";
    constexpr auto name() const -> std::string_view override { return id; }

    using Component::Component;
    virtual ~CommandStore() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto install_argument( Argument const& arg )
        -> Result< Uuid >;
    auto install_command( Command const& cmd )
        -> Result< Uuid >;
    auto install_guard( Guard const& guard )
        -> Result< Uuid >;
    auto install_standard_arguments()
        -> Result< void >;
    auto install_standard_guards()
        -> Result< void >;
    auto is_argument( Uuid const& node )
        -> bool;
    auto is_command( Uuid const& node )
        -> bool;
    auto is_guard( Uuid const& node )
        -> bool;
    auto uninstall_argument( Uuid const& argn )
        -> Result< void >;
    auto uninstall_command( Uuid const& cmdn )
        -> Result< void >;
    auto uninstall_guard( Uuid const& guardn )
        -> Result< void >;
};

} // namespace kmap::com

namespace kmap::view2::cmd
{
    auto const cli_root = anchor::abs_root | view2::direct_desc( "meta.setting.cli" );
    auto const command_root = cli_root | view2::child( "command" );
    auto const argument_root = cli_root | view2::child( "argument" );
    auto const guard_root = cli_root | view2::child( "guard" );
    auto const command = view2::all_of( view2::child, { "action", "argument", "description", "guard" } ) | view2::parent;
    auto const guard = view2::all_of( view2::child, { "action", "description" } ) | view2::parent;
}

#endif // KMAP_COM_COMMAND_HPP
