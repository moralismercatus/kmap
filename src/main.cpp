/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"
#include "cmd.hpp"
#include "common.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "js_iface.hpp"
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

#include <boost/stacktrace.hpp>
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
                                    alert( message.join( '\n' ) );
                                    return true;
                                };)%%%" );
}

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

    auto& kmap = Singleton::instance();
    
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

    if( auto const res = kmap.reset()
      ; !res )
    {
        KMAP_THROW_EXCEPTION_MSG( to_string( res.error() ) );
    }
}

auto register_arguments()
    -> void
{
    KMAP_LOG_LINE();
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
    KMAP_LOG_LINE();
}

// TODO: Prereq: kmap is initialized.
auto register_commands()
    -> void
{
    KMAP_LOG_LINE();
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
    KMAP_LOG_LINE();
}

auto reset_registrations()
    -> void
{
    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();

KMAP_LOG_LINE();
    if( auto const r = kmap.cli().reset_all_preregistered()
      ; !r )
    {
        KMAP_THROW_EXCEPTION_MSG( to_string( r.error() ) );
    }
KMAP_LOG_LINE();
    // Legacy commands... TODO: Remove all when legacy are transitioned to new method.
    for( auto const& c : cmd::make_core_commands( kmap ) )
    {
        cli.register_command( c );
    }
KMAP_LOG_LINE();
}

auto focus_network()
    -> void
{
KMAP_LOG_LINE();
    auto& kmap = Singleton::instance();

KMAP_LOG_LINE();
    kmap.select_node( kmap.root_node_id() );
KMAP_LOG_LINE();
}

auto set_window_title()
    -> Result< void >
{
#if KMAP_DEBUG
    return js::eval_void( io::format( "document.title = 'Knowledge Map {} {}';", "0.0.1", "D" ) );
#else
    return js::eval_void( io::format( "document.title = 'Knowledge Map {} {}';", "0.0.1", "R" ) );
#endif // KMAP_DEBUG
}

auto initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( init_js_syntax_error_handler() );
    init_ems_nodefs();
    configure_terminate();
    configure_contract_failure_handlers();
    init_kmap();
    register_arguments();
    register_commands();
    reset_registrations();
    focus_network();
    KMAP_TRY( set_window_title() );

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

