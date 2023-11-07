/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/filesystem/filesystem.hpp>

#include <kmap.hpp>
#include <utility.hpp>

#include <boost/filesystem.hpp>
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
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    init_ems_nodefs();
    KTRY( install_standard_commands() );

    rv = outcome::success();

    return rv;
}

auto Filesystem::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

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
    KM_RESULT_PROLOG();
    
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
} // namespace anon

} // namespace kmap::com
