/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COM_CCLERK_HPP
#define KMAP_COM_CCLERK_HPP

#include "common.hpp"
#include "component.hpp"
#include "com/cmd/command.hpp"

#include <map>
#include <set>
#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class CommandClerk
{
public:
    Kmap& kmap;
    std::map< std::string, Argument > registered_arguments = {};
    std::map< std::string, Command > registered_commands = {};
    std::map< std::string, CommandAlias > registered_command_aliases = {};
    std::map< std::string, Guard > registered_guards = {};
    std::map< std::string, Uuid > installed_arguments = {};
    std::map< std::string, Uuid > installed_commands = {};
    std::map< std::string, Uuid > installed_command_aliases = {};
    std::map< std::string, Uuid > installed_guards = {};

    CommandClerk( Kmap& km );
    ~CommandClerk();

    auto check_registered()
        -> Result< void >;
    auto check_registered( Argument const& arg )
        -> Result< void >;
    auto check_registered( Command const& cmd )
        -> Result< void >;
    auto check_registered( CommandAlias const& ca )
        -> Result< void >;
    auto check_registered( Guard const& guard )
        -> Result< void >;

    auto install_registered()
        -> Result< void >;

    auto register_argument( Argument const& arg ) 
        -> Result< void >;
    auto register_command( Command const& cmd ) 
        -> Result< void >;
    auto register_command_alias( CommandAlias const& ca ) 
        -> Result< void >;
    auto register_guard( Guard const& guard ) 
        -> Result< void >;

protected:
    auto install_argument( Argument const& arg )
        -> Result< Uuid >;
    auto install_command( Command const& cmd )
        -> Result< Uuid >;
    auto install_command_alias( CommandAlias const& ca )
        -> Result< Uuid >;
    auto install_guard( Guard const& guard )
        -> Result< Uuid >;
};

} // namespace kmap::com

#endif // KMAP_COM_CCLERK_HPP
