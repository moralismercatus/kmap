/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_SCRIPT_HPP
#define KMAP_CMD_SCRIPT_HPP

#include "com/cli/cli.hpp"

#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto load_script( Kmap& kmap 
                , std::istream& is )
    -> Result< std::string >;

} // namespace kmap::cmd

#endif // KMAP_CMD_CARDINALITY_HPP
