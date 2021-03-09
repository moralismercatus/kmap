/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_STATE_HPP
#define KMAP_CMD_STATE_HPP

#include "../cli.hpp"

#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto copy_state( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto load_state( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto new_state( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto save_state( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_STATE_HPP
