/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"
#include "com/canvas/canvas.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/node_view.hpp"

#include <boost/test/unit_test.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>

using namespace kmap;
using namespace ranges;
namespace utf = boost::unit_test;

namespace kmap::test {

/******************************************************************************/
/* path */
/******************************************************************************/
// TODO: Why doesn't this depend on kmap_iface instead of vice versa?
BOOST_AUTO_TEST_SUITE( path 
                     ,
                     * utf::label( "env" ) // TODO: Rather, shouldn't this depends_on( "kmap_iface" )? Well, that would incur a cyclic dep., so need to resolve.
                     * utf::fixture< ClearMapFixture >() )
/******************************************************************************/
/* path/decide_path */
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( decide_path )

auto check_path( Kmap const& kmap
               , std::string const& path
               , std::vector< Uuid > const& matches )
    -> bool
{
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const ps = kmap::decide_path( kmap
                                     , kmap.root_node_id()
                                     , nw->selected_node()
                                     , path );

    if( matches.empty() )
    {
        return !ps.has_value();
    }
    else
    {
        if( !ps )
        {
            return false;
        }
        else if( ps.value().size() != matches.size() )
        {
            return false;
        }
        else
        {
            for( auto const& match : matches )
            {
                if( !ranges::contains( ps.value(), match ) )
                {
                    return false;
                }
            }
        }

        return true;
    }
}

BOOST_AUTO_TEST_CASE( start_state
                    ,
                    * utf::description( "initial path completions" ) 
                    * utf::fixture< ClearMapFixture >() )
{
    auto const& kmap = Singleton::instance();
    auto nodes = create_lineages( "/1.2.3"
                                , "/2.3"
                                , "/2.4" );

    BOOST_TEST( check_path( kmap, ",.", {} ) );
    BOOST_TEST( check_path( kmap, ".", { kmap.root_node_id() } ) );
    BOOST_TEST( check_path( kmap, "/", { kmap.root_node_id() } ) );
    BOOST_TEST( check_path( kmap, "1", { nodes[ "/1" ] } ) );
    BOOST_TEST( check_path( kmap, "2", { nodes[ "/1.2" ], nodes[ "/2" ] } ) );
    BOOST_TEST( check_path( kmap, "3", { nodes[ "/1.2.3" ], nodes[ "/2.3" ] } ) );
    BOOST_TEST( check_path( kmap, "5", {} ) );
}

BOOST_AUTO_TEST_CASE( post_start_state
                    , 
                    * utf::depends_on( "path/decide_path/start_state" )
                    * utf::fixture< ClearMapFixture >() )
{
    // TODO: Incorporate disambiguation syntax into tests.
    auto const& kmap = Singleton::instance();
    auto nodes = create_lineages( "/1.2.3"
                                , "/2.3"
                                , "/2.4" );

    BOOST_TEST( check_path( kmap, "/2", { nodes[ "/2" ] } ) );
    BOOST_TEST( check_path( kmap, "/5", {} ) );
    BOOST_TEST( check_path( kmap, "/3", {} ) ); 

    BOOST_TEST( check_path( kmap, "/2/4", {} ) ); 

    BOOST_TEST( check_path( kmap, "/1.2.3", { nodes[ "/1.2.3" ] } ) );
    BOOST_TEST( check_path( kmap, "/1,.2.3,.,.1.2..3,.,.2.", { nodes[ "/1.2" ] } ) );
    BOOST_TEST( check_path( kmap, "2.3,.,.,.1", { nodes[ "/1" ] } ) );
    BOOST_TEST( check_path( kmap, "3,.,.2", { nodes[ "/1.2" ], nodes[ "/2" ] } ) );
}

BOOST_AUTO_TEST_SUITE_END( /*decide_path*/ );
/******************************************************************************/
/* path/complete */
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( complete
                     ,
                     * utf::depends_on( "path/decide_path" ) )

auto check_completion( Kmap const& kmap
                     , Uuid const& root
                     , std::string const& path
                     , StringVec const& completions )
    -> bool
{
    auto rv = false;
    auto const pss = kmap::complete_selection( kmap
                                             , root
                                             , root
                                             , path ).value();
    auto const ps = pss
                  | views::transform( &CompletionNode::path )
                  | to< StringVec >();

    if( completions.empty() )
    {
        rv = ps.empty();
    }
    else
    {
        if( ps.size() != completions.size() )
        {
            io::print( "[ failed ] Size mismatch: expected:{} != actual:{}\n", ps.size(), completions.size() );

            rv = false;
        }
        else
        {
            for( auto const& completion : completions )
            {
                if( !ranges::contains( ps, completion ) )
                {
                    io::print( stderr, "[ failed ] Result set does not contain completion: '{}'\n", completion );
                    rv = false;
                    break;
                }
            }

            rv = true;
        }
    }

    if( !rv )
    {
        io::print( stderr
                 , "[ failed ] Completion check.\n[ failed ] Expected: {}\n[ failed ] Actual: {}\n"
                 , completions | views::join( ' ' ) | to< std::string >()
                 , ps | views::join( ' ' ) | to< std::string >() );
    }

    return rv;
}

auto check_completion( Kmap const& kmap
                     , std::string const& path
                     , StringVec const& completions )
    -> bool
{
    return check_completion( kmap
                           , kmap.root_node_id()
                           , path
                           , completions );
}

BOOST_AUTO_TEST_CASE( /*path/complete*/start_state
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto nodes = create_lineages( "/test"
                                , "/test.zulu"
                                , "/test.foo"
                                , "/test.foobar" );
    auto const root = nodes[ "/test" ];

    BOOST_TEST( check_completion( kmap, root, "/", { "/zulu", "/foo", "/foobar" } ) );
    BOOST_TEST( check_completion( kmap, root, "z", { "zulu" } ) );
    BOOST_TEST( check_completion( kmap, root, "fo", { "foo", "foobar" } ) );
    BOOST_TEST( check_completion( kmap, root, "foo", { "foo", "foo,", "foobar" } ) );
}

BOOST_AUTO_TEST_CASE( /*path/complete*/next_state
                    ,
                    * utf::depends_on( "path/complete/start_state" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto nodes = create_lineages( "/zulu"
                                , "/foo"
                                , "/foobar.charlie" );

    BOOST_TEST( check_completion( kmap, "foobar", { "foobar", "foobar,", "foobar." } ) );
    BOOST_TEST( check_completion( kmap, "zu.", { "zulu" } ) );
}

BOOST_AUTO_TEST_CASE( /*path/complete*/disambiguate
                    ,
                    * utf::depends_on( "path/complete/next_state" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto nodes = create_lineages( "/1.zulu"
                                , "/1.2.zulu"
                                , "/3.2.zulu"
                                , "/4.zulu.5" );

    // BOOST_TEST( check_completion( kmap, "zu", { "zulu'1", "zulu'2'1", "zulu'2'3", "zulu'4" } ) );
    // BOOST_TEST( check_completion( kmap, "zulu", { "zulu'1", "zulu'2'1", "zulu'2'3", "zulu'4" } ) );
    // BOOST_TEST( check_completion( kmap, "zulu'", { "zulu'1", "zulu'2'1", "zulu'2'3", "zulu'4" } ) );
    // BOOST_TEST( check_completion( kmap, "zulu'1", { "zulu'1," } ) );
    // BOOST_TEST( check_completion( kmap, "zulu'2", { "zulu'2'1", "zulu'2'3" } ) );
    BOOST_TEST( check_completion( kmap, "zulu'2'3", { "zulu'2'3" } ) );
    // BOOST_TEST( check_completion( kmap, "zulu'4", { "zulu'4", "zulu'4,", "zulu'4." } ) );
    BOOST_TEST( check_completion( kmap, "zulu'4,", { "zulu'4,4" } ) );
    BOOST_TEST( check_completion( kmap, "zulu'4,4.zulu.", { "zulu'4,4.zulu.5" } ) );
    BOOST_TEST( check_completion( kmap, "2'2", { "2'1", "2'3" } ) );
    // BOOST_TEST( check_completion( kmap, "zulu,", { "zulu'1'root", "zulu,'1", "zulu,'3", "zulu'4'root" } ) ); // Tricky case where "zulu,root" is ambiguous and cannot be expanded with ' to disambiguate; only with replacement of ',' can it be disambiguated.
}

BOOST_AUTO_TEST_CASE( /*path/complete*/root_child
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto nodes = create_lineages( "/zulu"
                                , "/foo"
                                , "/foobar" );

    BOOST_TEST( check_completion( kmap, "/z", { "/zulu" } ) );
    BOOST_TEST( check_completion( kmap, "/f", { "/foo", "/foobar" } ) );
    BOOST_TEST( check_completion( kmap, "/", { "/zulu", "/foo", "/foobar", "/meta" } ) );
    BOOST_TEST( check_completion( kmap, "/a", { "/zulu", "/foo", "/foobar", "/meta" } ) );
}

BOOST_AUTO_TEST_CASE( /*path/complete*/any_lead
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    // auto& kmap = Singleton::instance();

    // TODO....

    create_lineages( "/unus"
                   , "/undecim"
                   , "/duodecim"
                   , "/duodecim"
                   , "/tribus"
                   , "/quattuor" );

    BOOST_TEST( true );
}

BOOST_AUTO_TEST_SUITE_END( /*path/complete*/ )

BOOST_AUTO_TEST_CASE( fetch_abs_root
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "/" )
              . has_value() );
}

BOOST_AUTO_TEST_CASE( fetch_abs_descendant
                    , 
                    * utf::depends_on( "path/fetch_abs_root" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    create_lineages( "/1.2.3" );

    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , nw->selected_node()
                                       , kmap.root_node_id()
                                       , "/1.2.3" )
              . has_value() == true );
}

BOOST_AUTO_TEST_CASE( fetch_rel_root
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "root" )
              . has_value() );
}

BOOST_AUTO_TEST_CASE( fetch_rel_single
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    create_lineages( "/1.2.3" );

    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "1" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "2" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "3" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "z" )
              . has_value() == false );
}

BOOST_AUTO_TEST_CASE( fetch_rel_mult_fwd 
                    , 
                    * utf::depends_on( "path/fetch_rel_single" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    
    create_lineages( "/1.2.3"
                   , "/4.5.6" );

    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "1.2" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "1.2.3" )
              . has_value() );
}

// BOOST_AUTO_TEST_SUITE_END( /*path/fetch*/ )

BOOST_AUTO_TEST_SUITE( bwd )

BOOST_AUTO_TEST_CASE( root_parent
                    , 
                    * utf::depends_on( "path/fetch_rel_mult_fwd" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    create_lineages( "/1.2.3"
                   , "/4.5.6" );
    
    BOOST_TEST( !kmap::fetch_descendants( kmap
                                        , kmap.root_node_id()
                                        , nw->selected_node()
                                        , "," ) );
    BOOST_TEST( !kmap::fetch_descendants( kmap
                                        , kmap.root_node_id()
                                        , nw->selected_node()
                                        , ",," ) );
    BOOST_TEST( !kmap::fetch_descendants( kmap
                                        , kmap.root_node_id()
                                        , nw->selected_node()
                                        , ",,," ) );
}

BOOST_AUTO_TEST_CASE( sel_parent
                    , 
                    * utf::depends_on( "path/fetch_rel_mult_fwd" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    
    create_lineages( "/1.2.3"
                   , "/4.5.6" );

    BOOST_TEST( nw->select_node( KTRYE( kmap.root_view() | view::direct_desc( "1.2.3" ) | view::fetch_node( kmap ) ) ) );

    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "," ) );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , ",," ) );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , ",,," ) );
    BOOST_TEST( !kmap::fetch_descendants( kmap
                                        , kmap.root_node_id()
                                        , nw->selected_node()
                                        , ",,,," ) );
}

BOOST_AUTO_TEST_CASE( mult_bwd
                    , 
                    * utf::depends_on( "path/fetch_rel_mult_fwd" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    
    create_lineages( "/1.2.3"
                   , "/4.5.6" );

    BOOST_TEST( nw->select_node( KTRYE( kmap.root_view() | view::direct_desc( "1.2.3" ) | view::fetch_node( kmap ) ) ) );

    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , ",," ) );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , ",,," ) );
    BOOST_TEST( !kmap::fetch_descendants( kmap
                                        , kmap.root_node_id()
                                        , nw->selected_node()
                                        , ",,,," ) );
}

BOOST_AUTO_TEST_CASE( rel_mult_bwd
                    , 
                    * utf::depends_on( "path/fetch_rel_mult_fwd" )
                    * utf::fixture< ClearMapFixture >() )
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    
    create_lineages( "/1.2.3"
                   , "/4.5.6"
                   , "/7.5.6" );

    BOOST_TEST( !kmap::fetch_descendants( kmap
                                        , kmap.root_node_id()
                                        , nw->selected_node()
                                        , "2,3" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "3,2" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "3,2,1" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "6,5,4" )
              . has_value() );
    BOOST_TEST( kmap::fetch_descendants( kmap
                                       , kmap.root_node_id()
                                       , nw->selected_node()
                                       , "6,5" )
              . has_value() );
}

BOOST_AUTO_TEST_SUITE_END( /*path/bwd*/ )
BOOST_AUTO_TEST_SUITE_END( /*path*/ )

} // namespace kmap::test