/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <common.hpp>

#include <error/filesystem.hpp>

#include <filesystem>
#include <istream>
#include <string>

namespace kmap {

auto init_ems_nodefs()
    -> void;
auto stream_size( std::istream& fs )
    -> std::istream::pos_type;
auto to_string( std::istream const& is )
    -> std::string;

// TODO: This belongs in com::Filesystem, to ensure FS is initialized before use.
auto open_ifstream( FsPath const& path
                  , std::ios_base::openmode const& om )
    -> Result< std::ifstream >;
// TODO: This belongs in com::Filesystem, to ensure FS is initialized before use.
auto open_ofstream( FsPath const& path
                  , std::ios_base::openmode const& om )
    -> Result< std::ofstream >;

} // namespace kmap