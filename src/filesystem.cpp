/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "filesystem.hpp"

#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/transform.hpp>

namespace fs = boost::filesystem;
using namespace ranges;

namespace kmap {

// TODO: I've read that this isn't a bulletproof way to determine size of the stream. Re-evaluate.
auto stream_size( std::istream& fs )
    -> std::istream::pos_type
{
    auto const existing = fs.tellg();
    fs.seekg( 0, std::ios::beg );
    auto const begin = fs.tellg();
    fs.seekg( 0, std::ios::end );
    auto const end = fs.tellg();
    auto const size = end - begin;

    fs.seekg( existing );

    return size;
}

} // namespace kmap