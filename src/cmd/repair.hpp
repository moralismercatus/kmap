/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_REPAIR_HPP
#define KMAP_CMD_REPAIR_HPP

#include "com/cli/cli.hpp"
#include "../common.hpp"

#include <functional>

namespace kmap {
class Kmap;
class Database;
}

namespace kmap::cmd::repair {

auto back_up_state( FsPath const& fp )
    -> void;
auto repair_state( FsPath const& path )
    -> Result< void >;
auto repair_conflicting_headings( Database& db )
    -> void;
auto repair_orderings( Database& db )
    -> void;

} // namespace kmap::cmd::repair

#endif // KMAP_CMD_REPAIR_HPP
