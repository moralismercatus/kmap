/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_UPDATE_BODY_HPP
#define KMAP_PATH_ACT_UPDATE_BODY_HPP

#include "common.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct UpdateBody
{
    Kmap& km;
    std::string content;

    UpdateBody( Kmap& kmap
              , std::string const& body );
};

auto update_body( Kmap& kmap
                , std::string const& body )
    -> UpdateBody;

auto operator|( Intermediary const& i, UpdateBody const& op )
    -> Result< void >;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}

#endif // KMAP_PATH_ACT_UPDATE_BODY_HPP
