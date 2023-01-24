/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_FETCH_HEADING_HPP
#define KMAP_PATH_ACT_FETCH_HEADING_HPP

#include "common.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct FetchHeading
{
    Kmap const& km;
};

auto fetch_heading( Kmap const& km )
    -> FetchHeading;

auto operator|( Intermediary const& i, FetchHeading const& op )
    -> Result< std::string >;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}

#endif // KMAP_PATH_ACT_FETCH_HEADING_HPP
