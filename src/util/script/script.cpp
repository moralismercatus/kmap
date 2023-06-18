/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "script.hpp"

#include "common.hpp"
#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "error/filesystem.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "cmd/command.hpp"
#include "com/cmd/command.hpp"
#include "com/filesystem/filesystem.hpp"

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>

#include <fstream>
#include <istream>

namespace fs = boost::filesystem;

namespace kmap::util {

auto load_script( Kmap& kmap 
                , std::istream& is )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto line = std::string{};

    while( std::getline( is, line ) )
    {
        auto const cli = KTRY( kmap.fetch_component< com::Cli >() );

        KTRY( cli->parse_raw( line ) );
    }

    rv = outcome::success();

    return rv;
}

auto load_script( Kmap& kmap
                , FsPath const& raw_path )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", raw_path.string() );

    auto rv = result::make_result< std::string >();
    auto const path = com::kmap_root_dir / raw_path;

    if( fs::exists( path ) )
    {
        auto ifs = std::ifstream{ path.string() };
        
        if( ifs.good() )
        {
            KMAP_TRY( load_script( kmap, ifs ) );

            rv = std::string{ "script executed" };
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::filesystem::file_read_failure );
        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::filesystem::file_not_found );
    }

    return rv;
}

auto to_js_body_code( std::string const& raw_code )
    -> std::string
{
    return fmt::format( "```javascript\n{}\n```", raw_code );
}

auto to_kscript_body_code( std::string const& raw_code )
    -> std::string
{
    return fmt::format( "```kscript\n{}\n```", raw_code );
}


} // namespace kmap::util

#if 0
namespace kmap::cmd::load_kscript_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( "unconditional" );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;

rv = kmap.load_kscript( args.get( 0 ) );

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "loads and executes kscript file";
auto const arguments = std::vector< Argument >{ Argument{ "script_file_path"
                                                        , "path to script file"
                                                        , "filesystem_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    load.kscript
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace kmap::cmd::load_kscript_def
#endif // 0

namespace kmap::cmd::script::binding {

using namespace emscripten;

auto load_kscript( std::string const& fs_path )
    -> kmap::Result< std::string >
{
    auto& kmap = Singleton::instance();

    return util::load_script( kmap, fs_path );
}

EMSCRIPTEN_BINDINGS( kmap_script )
{
    function( "load_kscript", &kmap::cmd::script::binding::load_kscript );
}

}  // namespace kmap::cmd::script::binding

