/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_RECIPE_HPP
#define KMAP_CMD_RECIPE_HPP

#include "../cli.hpp"

#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto create_recipe( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto create_step( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto add_step( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto add_prerequisite( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto create_prerequisite( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_CONCLUSION_HPP
