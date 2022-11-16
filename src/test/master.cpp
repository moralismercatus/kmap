/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#define BOOST_TEST_MODULE kmap_unit_test
// Boost.Test config macros.
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#define BOOST_TEST_DISABLE_ALT_STACK
// unit_test.hpp must go after config macros.
#include <boost/filesystem.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/test/unit_test.hpp>
#include <catch2/catch_session.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/split.hpp>

#include "cmd/command.hpp"
#include "contract.hpp"
#include "com/database/db.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "js_iface.hpp"
#include "master.hpp"

using namespace kmap;
namespace utf = boost::unit_test;
namespace fs = boost::filesystem;
namespace rvs = ranges::views;

namespace kmap::test {

ResetInstanceFixture::ResetInstanceFixture()
{
    if( auto const res = Singleton::instance().initialize()
      ; !res )
    {
        KMAP_THROW_EXCEPTION_MSG( to_string( res.error() ) );
    }
}

ResetInstanceFixture::~ResetInstanceFixture()
{
    try
    {
        auto const& kmap = Singleton::instance();
        auto const db = KTRYE( kmap.fetch_component< com::Database >() );

        fs::remove( db->path() );
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

ClearMapFixture::ClearMapFixture()
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    for( auto const& c : nw->fetch_children( kmap.root_node_id() ) )
    {
        if( nw->fetch_heading( c ).value() != "meta" )
        {
            ( void )nw->erase_node( c );
        }
    }

    if( auto const res = nw->select_node( kmap.root_node_id() )
      ; !res )
    {
        KMAP_THROW_EXCEPTION_MSG( to_string( res.error() ) );
    }
}

ClearMapFixture::~ClearMapFixture()
{
    // Nothing to do here.
}

auto select_each_descendant_test( Kmap& kmap
                                , Uuid const& node )
    -> bool
{
    auto rv = view::make( node ) | view::exists( kmap );

    for( auto const& child : view::make( node )
                           | view::child
                           | view::to_node_set( kmap ) )
    {
        if( rv = select_each_descendant_test( kmap, child )
          ; !rv )
        {
            break;
        }
    }

    return rv;
}

} // namespace kmap::test

namespace kmap {

/**
 * Note Regarding Execution Order: Catch2 claims it orders tests, by default, in the following order:
 * Within Translation Units, in the order they are declared.
 * Between Translation Units, the order in which they're linked.
 * There does not appear to be any way to order based on explicit dependencies, e.g., utf::depends_on.
 * There is, I think, a way to force execution order based on tags "[<tag>]", by explicitly listening them via CLI args.
 * The advantage I'm holding out on, is that Translation Unit linking order is in the order of dependency;
 * thus, the dependent order that I desire happens automagically. That is the hope.
 */
auto run_pre_env_unit_tests()
    -> int
{
    {
        // Use -# [#<file>] without extension to unit test particular file.
        if( auto const specified_tests = js::eval< std::string >( "return kmap_pretest_targets;" )
          ; specified_tests )
        {
            auto const options = [ & ]
            {
                if( auto const opts = js::eval< std::string >( "return kmap_pretest_options;" )
                  ; opts )
                {
                    return opts.value()
                         | rvs::split( ' ' )
                         | ranges::to< std::vector< std::string > >();
                }
                return std::vector< std::string >{};
            }();
            auto argv = std::vector< char const* >{ "kmap" };
            for( auto const& opt : options )
            {
                argv.emplace_back( opt.c_str() );
            }
            argv.emplace_back( specified_tests.value().c_str() );

            io::print( "[log] Running Catch2 pre-env unit tests\n" );

            if( 0 != Catch::Session().run( argv.size(), argv.data() ) )
            {
                return 1;
            }
        }
        else
        {
            io::print( "[log] No pretest specified\n" );
        }
    }
    // TODO: So, the alternative, and probably better solution to running only non-UI "pre" tests, is to use the negation (assuming it works with labels)
    //       "--run_test=!@UI". The point about pre-test is that it doesn't depend on the environment being set up, so the user can isolate unit test
    //       problems without the environment, in addition to isolating whether a problem is in the env or not.
    //       Thus, maybe "pre" should be renamed "env", as the unit test depends on the environment being set up.
    //       Such pre-env testing wasn't even really possible, for the most part, given the dependency on the FS, but with in memory storage now, 
    char const* targv[] = { "kmap"
                          , "--run_test=!@env" // Run all tests without the "env" label.
                          , "--show_progress=true"
                          , "--report_level=detailed"
                          , "--log_level=all" };
    io::print( "[log] Running pre-env unit tests\n" );

    return utf::unit_test_main( init_unit_test
                              , sizeof( targv ) / sizeof( char* )
                              , const_cast< char** >( targv ) ); // WARNING: const_cast necessary b/c of non-const C-style interface.
}

auto run_unit_tests( StringVec const& args )
    -> int
{
    // KMAP_ENSURE_EXCEPT( !args.empty() );

    // auto targv = new char*[ args.size() ];

    // KMAP_LOG_LINE();
    // for( auto const& [ i, s ] : ranges::views::enumerate( args ) )
    // {
    //     auto ts = new char[ s.size() + 1 ];
    //     strcpy( ts, s.c_str() );
    //     ts[ s.size() ] = '\0';
    //     targv[ i ] = ts;
    // }
    // return utf::unit_test_main( init_unit_test
    //                           , targs.size() - 2
    //                           , &targv[ 0 ] );

    // TODO: This is simply perplexing to me. This is the only way to avoid a fault, it appears, to make the arguments static.
    //       I can't spend more time debugging this at the moment, so acquiescing to this. See above code for dyn args to the UTF.
    io::print( "[warning] Ignoring provided args. See code comments for details. Long story. Needs work.\n" );
    char const* targv[] = { "kmap"
                          , "--run_test=*" // Run all tests without the "env" label.
                          , "--show_progress=true"
                          , "--report_level=detailed"
                          , "--log_level=all" };

    try
    {
        return utf::unit_test_main( init_unit_test
                                  , sizeof( targv ) / sizeof( char* )
                                  , const_cast< char** >( targv ) ); // WARNING: const_cast necessary b/c of non-const C-style interface.
    }
    catch(const std::exception& e)
    {
        fmt::print( stderr, "unit tests threw an exception: {}\n", e.what() );
        throw;
    }
}

} // namesapce kmap
