/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_TEXT_HPP
#define KMAP_CMD_TEXT_HPP

#include "../cli.hpp"

#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto edit_body( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto view_body( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_TEXT_HPP
