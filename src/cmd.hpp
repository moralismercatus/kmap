/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_HPP
#define KMAP_CMD_HPP

#include "cli.hpp"

#include <string>
#include <vector>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto make_core_commands( Kmap& kmap )
    -> std::vector< CliCommand >;

} // namespace kmap::cmd

#endif // KMAP_CMD_HPP
