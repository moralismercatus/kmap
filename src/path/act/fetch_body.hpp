/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_FETCH_BODY_HPP
#define KMAP_PATH_ACT_FETCH_BODY_HPP

#include "common.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct FetchBody
{
    Kmap const& km;
};

auto fetch_body( Kmap const& km )
    -> FetchBody;

auto operator|( Intermediary const& i, FetchBody const& op )
    -> Result< std::string >;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}

#endif // KMAP_PATH_ACT_FETCH_BODY_HPP
