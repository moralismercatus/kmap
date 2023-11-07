/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/filesystem/filesystem.hpp>
#include <kmap.hpp>
#include <utility.hpp>

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <fstream>
#include <vector>

namespace bfs = boost::filesystem;

namespace kmap::com::binding {

namespace {

using namespace emscripten;

auto complete_filesystem_path( std::string const& raw )
    -> StringVec
{
    auto const parent_path = bfs::path{ raw }.parent_path();
    auto to_paths = ranges::views::transform( [ & ]( auto const& e )
    {
        return ( parent_path / e.path().filename() ).string();
    } );
    auto const di = bfs::directory_iterator{ com::kmap_root_dir / parent_path };
    auto const paths = di
                        | to_paths
                        | ranges::to_vector;

    return fetch_completions( raw, paths );
}

auto fs_path_exists( std::string const& path )
    -> bool 
{
    namespace fs = boost::filesystem;

    return fs::exists( com::kmap_root_dir / path );
}

EMSCRIPTEN_BINDINGS( kmap_filesystem )
{
    function( "complete_filesystem_path", &kmap::com::binding::complete_filesystem_path );
    function( "fs_path_exists", &kmap::com::binding::fs_path_exists );
}

} // namespace anon

} // namespace kmap::com
