/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_SELECT_NODE_HPP
#define KMAP_PATH_ACT_SELECT_NODE_HPP

#include "common.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct SelectNode
{
    Kmap& km;
};

auto select_node( Kmap& km )
    -> SelectNode;

auto operator|( Intermediary const& i, SelectNode const& op )
    -> Result< void >;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}

#endif // KMAP_PATH_ACT_SELECT_NODE_HPP
