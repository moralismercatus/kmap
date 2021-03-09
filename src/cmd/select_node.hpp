/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_SELECT_NODE_HPP
#define KMAP_CMD_SELECT_NODE_HPP

#include "../cli.hpp"

#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto select_destination( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto select_node( Kmap& kmap
                , std::string const& dst )
    -> Result< std::string >;
auto select_node( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto select_root( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto select_source( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_SELECT_NODE_HPP
