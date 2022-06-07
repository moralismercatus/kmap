/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_TO_STRING_HPP
#define KMAP_PATH_ACT_TO_STRING_HPP

#include "path/node_view.hpp"
#include "utility.hpp"

#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct ToString
{
    ToString() = default;
};

auto const to_string = ToString{};

auto operator|( Intermediary const& i, ToString const& op )
    -> std::string;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}


#endif // KMAP_PATH_ACT_TO_STRING_HPP
