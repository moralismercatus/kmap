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

#include <sqlpp11/sqlite3/connection.h>

#include <functional>

namespace kmap {
class Kmap;
class Database;
}

namespace kmap::repair {

auto back_up_state( FsPath const& fp ) -> Result< void >;
auto erase_node( sqlpp::sqlite3::connection& con, std::string const& node ) -> Result< void >;
auto repair_conflicting_headings( Database& db ) -> void;
auto repair_map( FsPath const &fp ) -> Result< void >;
auto repair_orderings( Database& db ) -> void;

} // namespace kmap::repair

#endif // KMAP_CMD_REPAIR_HPP
