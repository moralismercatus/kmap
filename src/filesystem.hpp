/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "common.hpp"

#include <fstream>

namespace kmap {

auto complete_filesystem_path( std::string const& raw )
    -> StringVec;
auto stream_size( std::istream& fs )
    -> std::istream::pos_type;

} // namespace kmap