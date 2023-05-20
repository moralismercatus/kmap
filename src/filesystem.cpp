/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <filesystem.hpp>

#include <utility.hpp>
#include <util/result.hpp>

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/transform.hpp>

#include <filesystem>
#include <fstream>

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

auto to_string( std::istream const& is )
    -> std::string
{
    auto ss = std::stringstream{};
    
    ss << is.rdbuf();

    return ss.str();
}

auto open_ifstream( FsPath const& path
                  , std::ios_base::openmode const& om )
    -> Result< std::ifstream >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path.string() );

    auto rv = result::make_result< std::ifstream >();

    KMAP_ENSURE( fs::exists( path ), error_code::filesystem::file_not_found );

    auto ifs = std::ifstream{ path.string(), om | std::ios_base::in };

    KMAP_ENSURE( ifs.good(), error_code::filesystem::file_open_failed );

    rv = std::move( ifs );

    return rv;
}

// TODO: This belongs in com::Filesystem, to ensure FS is initialized before use.
auto open_ofstream( FsPath const& path
                  , std::ios_base::openmode const& om )
    -> Result< std::ofstream >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path.string() );

    auto rv = result::make_result< std::ofstream >();

    KMAP_ENSURE( fs::exists( path ), error_code::filesystem::file_not_found );

    auto ofs = std::ofstream{ path.string(), om | std::ios_base::out };

    KMAP_ENSURE( ofs.good(), error_code::filesystem::file_open_failed );

    rv = std::move( ofs );

    return rv;
}

} // namespace kmap