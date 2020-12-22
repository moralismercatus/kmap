/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_SEARCH_HPP
#define KMAP_CMD_SEARCH_HPP

#include "../cli.hpp"

#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

inline auto const searches_root = std::string{ "/meta.searches" };

auto search_bodies( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto search_bodies_first( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto search_leaf_bodies( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;
auto search_headings( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_SEARCH_HPP
