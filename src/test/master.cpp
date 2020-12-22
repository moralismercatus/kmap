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
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional/optional_io.hpp>

#include "../cmd/command.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "../path.hpp"
#include "master.hpp"

using namespace kmap;
namespace utf = boost::unit_test;
namespace fs = boost::filesystem;

namespace kmap::test {

ResetInstanceFixture::ResetInstanceFixture()
{
    Singleton::instance().reset();
}

ResetInstanceFixture::~ResetInstanceFixture()
{
    fs::remove( Singleton::instance().database().path() );
}

ClearMapFixture::ClearMapFixture()
{
    auto& kmap = Singleton::instance();

    for( auto const& c : kmap.fetch_children( kmap.root_node_id() ) )
    {
        if( kmap.fetch_heading( c ).value() != "meta" )
        {
            ( void )kmap.delete_node( c );
        }
    }

    kmap.select_node( kmap.root_node_id() );
}

ClearMapFixture::~ClearMapFixture()
{
    // Nothing to do here.
}

auto select_each_descendant_test( Kmap& kmap
                                , Uuid const& node )
    -> bool
{
    auto rv = kmap.select_node( node );

    for( auto const& child : kmap.fetch_children( node ) )
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

auto run_unit_tests( /*std::vector< std::string > const& args*/ )
    -> int
{
    // TODO: Having difficulty communicating argc/argv arguments to
    // unit_test_main. What is is what works. Figure this out.
    auto targs = StringVec{};//args;
    auto argv = [ & ]
    {
        auto rv = std::vector< char const* >{};

        for( auto& e : targs )
        {
            rv.emplace_back( e.c_str() );
        }

        return rv;
    }();
    
    // char const* targv = "--show_progress=true";
    char const* targv[] = { "kmap"
                          , "--show_progress=true"
                          , "--report_level=detailed"
                          , "--log_level=all" };

    return utf::unit_test_main( init_unit_test
                              , sizeof( targv ) / sizeof( char* )
                              , const_cast< char** >( targv ) ); // WARNING: const_cast necessary b/c of non-const C-style interface.
                              // , argv.size()
                              // , &targv/* const_cast< char** >( argv.data() ) */ ); // WARNING: const_cast necessary b/c of non-const C-style interface.
}

namespace cmd::run_unit_tests {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
return kmap.run_unit_tests();
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "runs all build-in unit tests";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    run.unit_tests
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace cmd::run_unit_tests

} // namesapce kmap
