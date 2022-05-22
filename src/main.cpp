/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"
#include "cmd.hpp"
#include "common.hpp"
#include "contract.hpp"
#include "error/network.hpp"
#include "filesystem.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "test/master.hpp"
#include "utility.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <range/v3/view/enumerate.hpp>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/ppgen.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlite3/connection.h>

// #include <boost/stacktrace.hpp> // Note: Until emscrption supports libunwind, use print_stacktrace() which uses JS's stack trace dump. Works well.
#include <sstream>

#include <fstream>
#include <string>
#include <vector>

using namespace emscripten;
using namespace ranges;
using namespace kmap;

namespace {

auto init_js_syntax_error_handler()
    -> Result< void >
{
    // TODO: error info is just about useless. Line number is 1.... does window.addEventListener( 'error')
    return js::eval_void( R"%%%(window.onerror = function( msg, url, lineNo, columnNo, error )
                                {
                                    let message = [ 'Caught by `window.onerror` handler. Major error occurred. Likely uncaught exception or uncatchable JS Syntax Error.'
                                                  , 'Message: ' + msg
                                                  , 'URL: ' + url
                                                  , 'Line: ' + lineNo
                                                  , 'Column: ' + columnNo
                                                  , 'Error object: ' + JSON.stringify( error ) ];
                                    console.log( message.join( '\n' ) );
                                    if( is_cpp_exception( error ) )
                                    {
                                        kmap.print_std_exception( error );
                                    }
                                    alert( message.join( '\n' ) );
                                    return true;
                                };)%%%" );
}

// auto init_ems_nodefs()
//     -> void
// {
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
// }

auto init_kmap()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& kmap = Singleton::instance();

    // TODO: Why isn't this working?
    // KMAP_ENSURE_MSG( !js::is_global_kmap_null(), error_code::network::invalid_instance, "expected JS kmap to be non-null" );

    KMAP_TRY( kmap.reset() );

    rv = outcome::success();

    return rv;
}

auto register_arguments()
    -> void
{
    for( auto const& arg : vecFromJSArray< std::string >( val::global( "registration_arguments" ) ) )
    {
        if( auto const& succ = js::eval_void( fmt::format( "Module.register_arg_{}();", arg ) )
          ; !succ )
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
        if( auto const& succ = js::eval_void( fmt::format( "Module.register_cmd_{}();", cmd ) )
          ; !succ )
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
    auto& cli = kmap.cli();

    if( auto const r = kmap.cli().reset_all_preregistered()
      ; !r )
    {
        KMAP_THROW_EXCEPTION_MSG( to_string( r.error() ) );
    }
    // Legacy commands... TODO: Remove all when legacy are transitioned to new method.
    for( auto const& c : cmd::make_core_commands( kmap ) )
    {
        cli.register_command( c );
    }
}

auto focus_network()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& kmap = Singleton::instance();

    KMAP_TRY( kmap.select_node( kmap.root_node_id() ) );

    rv = outcome::success();

    return rv;
}

auto set_window_title() // TODO: Move definition to canvas? Basically, elsewhere.
    -> Result< void >
{
#if KMAP_DEBUG
    return js::eval_void( io::format( "document.title = 'Knowledge Map {} {}';", "0.0.1", "Debug" ) );
#else
    return js::eval_void( io::format( "document.title = 'Knowledge Map {} {}';", "0.0.1", "Release" ) );
#endif // KMAP_DEBUG
}

auto initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( init_js_syntax_error_handler() );
    configure_terminate();
    configure_contract_failure_handlers();
    KMAP_TRY( init_kmap() );
    register_arguments();
    register_commands();
    reset_registrations();
    KMAP_TRY( focus_network() );

    {
        // TODO:
        // Hmm... So, compiling this causes a linker failure: undefined symbol: _Unwind_GetIP 
        // _Unwind_GetIP normally lives in libunwind.
        // According to github, emscripten has implemented the stubs for libunwind, so we should be in the clear.
        // The "latest" 2.0.9 indeed claims to have libunwind.a (if you search its dir tree), wheras
        // the current used version (1.39.0) has the header files for libunwind, but no .a, which would indicate why linking is failing.
        // It may be worthwhile to attempt to use the latest version, now.
        // If I could get meaningful stack traces, that'd be fantastic. I could override the default Result NoValuePolicy and get a stack trace for every time that occurs.
        // std::stringstream ss;
        // ss << boost::stacktrace::stacktrace();
        // fmt::print( "stacktrac: {}\n", ss.str() );
    }

    rv = outcome::success();

    return rv;
}

} // anonymous ns

auto main( int argc
         , char* argv[] )
    -> int
{
    try
    {
        KMAP_TRYE( set_window_title() );
        init_ems_nodefs();
        js::set_global_kmap( Singleton::instance() );

#if KMAP_TEST_PRE_ENV 
        if( auto const res = run_pre_env_unit_tests()
          ; res == 0 )
        {
            fmt::print( "[log][test] Pre-environment test succeeded\n" );
        }
        else
        {
            fmt::print( stderr, "[error][test] Pre-environment test failed!\n" );

            return -1;
        }
#endif // KMAP_TEST_PRE_ENV

        if( auto const res = initialize()
          ; !res )
        {
            KMAP_THROW_EXCEPTION_MSG( to_string( res.error() ) );
        }
        // Ensure code & globals persist past scope exit.
        emscripten_exit_with_live_runtime(); 
    }
    catch( std::exception const& e )
    {
        io::print( stderr
                 , "exception: {}\n"
                 , e.what() );
    }

    return 0;
}

