/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_ERASE_HPP
#define KMAP_PATH_ACT_ERASE_HPP

#include "common.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct Erase
{
    Kmap& km;
};

auto erase( Kmap& kmap )
    -> Erase;

auto operator|( Intermediary const& i, Erase const& op )
    -> Result< void >;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}

#endif // KMAP_PATH_ACT_ERASE_HPP
