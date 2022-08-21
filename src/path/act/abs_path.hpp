/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ACT_ABS_PATH_HPP
#define KMAP_PATH_ACT_ABS_PATH_HPP

#include "common.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::view::act {

struct AbsPath 
{
    Kmap const& km;

    AbsPath( Kmap const& kmap );
};
struct AbsPathFlat
{
    Kmap const& km;

    AbsPathFlat( Kmap const& kmap );
};

auto abs_path( Kmap const& km )
    -> AbsPath;
auto abs_path_flat( Kmap const& km )
    -> AbsPathFlat;

auto operator|( Intermediary const& i, AbsPath const& op )
    -> Result< UuidVec >;
auto operator|( Intermediary const& i, AbsPathFlat const& op )
    -> Result< std::string >;

} // namespace kmap::act

namespace kmap
{
    namespace act = kmap::view::act;
}

#endif // KMAP_PATH_ACT_ABS_PATH_HPP
