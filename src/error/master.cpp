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
            static auto os = fsys.value()->open_ofstream( "kmap.log.ktry.line.txt" );

            KMAP_ENSURE_EXCEPT( os.good() );

            os << fmt::format( "[log.line] {}|{}|{}\n", func, line, FsPath{ file }.filename().string() );

            os.flush();
        }
    }
}

} // namespace kmap