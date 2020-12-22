/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"
#include "common.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "utility.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <range/v3/view/enumerate.hpp>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/ppgen.h>
#include <sqlpp11//sqlite3/sqlite3.h>
#include <sqlpp11//sqlite3/connection.h>

#include <fstream>
#include <string>
#include <vector>

using namespace emscripten;
using namespace ranges;
using namespace kmap;

namespace {

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

auto init_kmap()
    -> void
{
    using emscripten::val;

    Singleton::instance();
    
    val::global().call< val >( "eval"
                             , std::string{ "global.kmap = Module;" } );

    if( auto const res = val::global().call< val >( "eval"
                                                  , std::string{ "kmap != null" } ) // TODO: Figure out how to make it so kmap can't be reassigned.
      ; res.as< bool >() )
    {
        fmt::print( "kmap module initialized\n" );
    }
    else
    {
        fmt::print( stderr, "Unable to initialize kmap module\n" );
    }
}

auto register_arguments()
    -> void
{
    for( auto const& arg : vecFromJSArray< std::string >( val::global( "registration_arguments" ) ) )
    {
        if( auto const& succ = val::global().call< val >( "eval_code"
                                                        , fmt::format( "Module.register_arg_{}();"
                                                                     , arg ) )
          ; succ.as< bool >() )
        {
            // fmt::print( "successfully registered arg: {}\n"
            //           , arg );
        }
        else
        {
            fmt::print( stderr
                      , "failed to register {}\n"
                      , arg );
        }
    }
}

// TODO: Prereq: kmap is initialized.
auto register_commands()
    -> void
{
    for( auto const& cmd : vecFromJSArray< std::string >( val::global( "registration_commands" ) ) )
    {
        if( auto const& succ = val::global().call< val >( "eval_code"
                                                        , fmt::format( "Module.register_cmd_{}();"
                                                                     , cmd ) )
          ; succ.as< bool >() )
        {
            // fmt::print( "successfully registered cmd: {}\n"
            //           , cmd );
        }
        else
        {
            fmt::print( stderr
                      , "failed to register {}\n"
                      , cmd );
        }
    }
}

auto reset_registrations()
    -> void
{
    auto& kmap = Singleton::instance();

    kmap.cli().reset_all_preregistered();
}

auto focus_network()
    -> void
{
    auto& kmap = Singleton::instance();

    kmap.select_node( kmap.root_node_id() );
}

auto initialize()
    -> void
{
    init_ems_nodefs();
    configure_terminate();
    configure_contract_failure_handlers();
    init_kmap();
    register_arguments();
    register_commands();
    reset_registrations();
    focus_network();
}

} // anonymous ns

auto main( int argc
         , char* argv[] )
    -> int
{
    initialize();
    // Ensure code & globals persist past scope exit.
    emscripten_exit_with_live_runtime(); 

    return 0;
}

