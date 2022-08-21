/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_NODE_MANIP_HPP
#define KMAP_CMD_NODE_MANIP_HPP

#include "com/cli/cli.hpp"
#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto create_child( Kmap& kmap
                 , Uuid const& parent
                 , Title const& title
                 , Heading const& heading )
    -> Result< std::string >;
auto merge_nodes( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto move_body( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto move_node( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto move_up( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto move_down( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto move_children( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto move_siblings( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_NODE_MANIP_HPP
