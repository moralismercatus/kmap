/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_TAG_HPP
#define KMAP_CMD_TAG_HPP

#include "../cli.hpp"

#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto create_tag( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;
auto add_tag( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_TAG_HPP
