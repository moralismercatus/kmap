/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "common.hpp"

#include "error/network.hpp"
#include "io.hpp"
#include "kmap.hpp"

namespace kmap {

auto root_dir()
    -> std::string
{
    return kmap_root_dir.string();
}

} // namespace kmap

auto operator<<( std::ostream& os
               , std::pair< boost::uuids::uuid, boost::uuids::uuid > const& lhs )
    -> std::ostream&
{
    os << '{' << lhs.first << ',' << lhs.second << '}';

    return os;
}
