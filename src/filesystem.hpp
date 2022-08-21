/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "common.hpp"

#include <fstream>

namespace kmap {

auto stream_size( std::istream& fs )
    -> std::istream::pos_type;
auto init_ems_nodefs()
    -> void;

} // namespace kmap