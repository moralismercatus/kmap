/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "error/master.hpp"

#include "com/filesystem/filesystem.hpp"
#include "filesystem.hpp"
#include "kmap.hpp"

#include <string>

namespace kmap {

auto format_line_log( std::string const& func
                    , uint32_t const& line
                    , std::string const& file )
    -> std::string
{
    return fmt::format( "[log.line] {}|{}|{}", func, line, FsPath{ file }.filename().string() );
}

auto log_ktry_line( std::string const& func
                  , uint32_t const& line
                  , std::string const& file )
    -> void
{
    auto& km = kmap::Singleton::instance();

    if( km.has_component_system() )
    {
        if( auto const fsys = km.fetch_component< com::Filesystem >()
          ; fsys )
        {
            auto const file_name = "kmap.log.ktry.line.txt";
            auto const linestr = fmt::format( "{}\n", format_line_log( func, line, file ) );

            static auto os = fsys.value()->open_ofstream( file_name );

            KMAP_ENSURE_EXCEPT( os.good() );

            os << linestr;

            os.flush();
        }
    }
}

auto log_ktrye_line( std::string const& func
                   , uint32_t const& line
                   , std::string const& file )
    -> void
{
    fmt::print( stderr, "{}|Exception Raised\n", format_line_log( func, line, file ) );
}

} // namespace kmap