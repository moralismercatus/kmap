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

auto complete_filesystem_path( std::string const& raw )
    -> StringVec
{
    auto const parent_path = fs::path{ raw }.parent_path();
    auto to_paths = views::transform( [ & ]( auto const& e )
    {
        return ( parent_path / e.path().filename() ).string();
    } );
    auto const di = fs::directory_iterator{ kmap_root_dir / parent_path };
    auto const paths = di
                     | to_paths
                     | to_vector;

    return fetch_completions( raw 
                            , paths );
}

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

auto init_ems_nodefs()
    -> void
{
#ifndef NODERAWFS
    EM_ASM({
        let rd = UTF8ToString( $0 );
        FS.mkdir( rd );
        FS.mount( NODEFS
                , { root: "." }
                , rd );

    }
    , kmap_root_dir.string().c_str() );
#else
    #error Unsupported
#endif // NODERAWFS
}

} // namespace kmap