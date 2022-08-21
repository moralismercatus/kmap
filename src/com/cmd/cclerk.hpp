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

#include <set>
#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

struct CommandClerk
{
    Kmap& kmap;
    std::vector< Uuid > arguments;
    std::vector< Uuid > commands;

    CommandClerk( Kmap& km );
    ~CommandClerk();

    auto install_argument( Argument const& arg )
        -> Result< Uuid >;
    auto install_command( Command const& cmd )
        -> Result< Uuid >;
};

} // namespace kmap::com

#endif // KMAP_COM_CCLERK_HPP
