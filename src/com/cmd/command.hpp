/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COM_COMMAND_HPP
#define KMAP_COM_COMMAND_HPP

#include "common.hpp"
#include "component.hpp"

#include <set>
#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

struct Command
{
    struct Argument
    {
        Heading heading;
        std::string description;
        Heading argument_alias;
    };
    struct Guard
    {
        Heading heading;
        std::string code;
    };

    std::string path;
    std::string description;
    std::vector< Argument > arguments;
    Guard guard;
    std::string action;
};
struct Argument
{
    std::string path;
    std::string description;
    std::string guard;
    std::string completion;
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

    auto argument_root()
        -> Result< Uuid >;
    auto command_root()
        -> Result< Uuid >;
    auto install_argument( Argument const& arg )
        -> Result< Uuid >;
    auto install_command( Command const& cmd )
        -> Result< Uuid >;
    auto install_standard_arguments()
        -> Result< void >;
    auto is_argument( Uuid const& node )
        -> bool;
    auto is_command( Uuid const& node )
        -> bool;
    auto uninstall_argument( Uuid const& argn )
        -> Result< void >;
    auto uninstall_command( Uuid const& cmdn )
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_COM_COMMAND_HPP
