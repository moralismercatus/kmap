/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_CONCLUSION_HPP
#define KMAP_CMD_CONCLUSION_HPP

#include "../cli.hpp"
#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto create_conclusion( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto create_premise( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto add_premise( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto create_objection( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto add_objection( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;

auto create_citation( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_CONCLUSION_HPP
