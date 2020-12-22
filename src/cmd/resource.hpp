/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_RESOURCE_HPP
#define KMAP_CMD_RESOURCE_HPP

#include "../cli.hpp"
#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto store_resource( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto store_url_resource( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto add_resource( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_RESOURCE_HPP
