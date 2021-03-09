/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_NEXT_FORK_HPP
#define KMAP_CMD_NEXT_FORK_HPP

#include "../cli.hpp"
#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto next_fork( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto prev_fork( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto next_leaf( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_NEXT_FORK_HPP
