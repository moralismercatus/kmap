/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "filesystem.hpp"

#include "kmap.hpp"
#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include <fstream>
#include <vector>

namespace bfs = boost::filesystem;

namespace kmap::com {

Filesystem::~Filesystem()
{
// TODO: This should work, but mounting and unmounting seems to cause problems...
// #ifndef NODERAWFS
//     EM_ASM({
//         let rd = UTF8ToString( $0 );
//         FS.unmount( rd );
//         FS.rmdir( rd );
//     }
//     , kmap_root_dir.string().c_str() );
// #else
//     #error Unsupported
// #endif // NODERAWFS
}

auto Filesystem::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    init_ems_nodefs();
    KTRY( install_standard_commands() );

    rv = outcome::success();

    return rv;
}

auto Filesystem::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto Filesystem::init_ems_nodefs()
    -> void
{
// TODO: This, combined with dtor, should mount and unmount FS, but unmount (or repeated mount, unmount) isn't cooperating,
//       so leaving FS mount to pre-kmap code.
// #ifndef NODERAWFS
//     EM_ASM({
//         let rd = UTF8ToString( $0 );
//         FS.mkdir( rd );
//         FS.mount( NODEFS
//                 , { root: "." }
//                 , rd );

//     }
//     , kmap_root_dir.string().c_str() );
// #else
//     #error Unsupported
// #endif // NODERAWFS
}

auto Filesystem::install_standard_commands()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto Filesystem::open_ifstream( std::string const& path )
    -> std::ifstream
{
    auto const full_path = kmap_root_dir / path;

    return std::ifstream( full_path.string() );
}

auto Filesystem::open_ofstream( std::string const& path )
    -> std::ofstream
{
    auto const full_path = kmap_root_dir / path;

    return std::ofstream( full_path.string() );
}

namespace {
namespace filesystem_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Filesystem
,   std::set({ "component_store"s })
,   "filesystem support"
);

} // namespace filesystem_def 

namespace binding
{
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
} // namespace binding
} // namespace anon

} // namespace kmap::com
