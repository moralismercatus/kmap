/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_DOTLANG_HPP
#define KMAP_CMD_DOTLANG_HPP

#include <functional>
#include "../cli.hpp"

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto load_dot( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >;

} // namespace kmap::cmd

#endif // KMAP_CMD_DOTLANG_HPP
