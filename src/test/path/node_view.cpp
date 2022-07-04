/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../kmap.hpp"
#include "../../path/node_view.hpp"
#include "../master.hpp"
#include "canvas.hpp"
#include "js_iface.hpp"
#include "test/util.hpp"

#include <boost/test/unit_test.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>

using namespace kmap;
using namespace kmap::test;

SCENARIO( "node_view view::exists", "[path][node_view]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = kmap::Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "root node" )
    {
        THEN( "root node exists" )
        {
            REQUIRE( view::make( root ) | view::exists( kmap ) );
        }
        THEN( "non-root does not exist" )
        {
            REQUIRE( !( view::make( gen_uuid() ) | view::exists( kmap ) ) );
        }
        THEN( "child of root doesn't exist" )
        {
            REQUIRE( !( view::make( root ) | view::child | view::exists( kmap ) ) );
        }
    }
}

SCENARIO( "node_view view::alias", "[path][node_view]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = kmap::Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "/1 and /2" )
    {
        auto const c1r = kmap.create_child( root, "1" ); REQUIRE( succ( c1r ) );
        auto const c2r = kmap.create_child( root, "2" ); REQUIRE( succ( c2r ) );
        auto const c1 = c1r.value();
        auto const c2 = c2r.value();

        WHEN( "view::alias( Uuid ) is used to create an alias" )
        {
            auto const a1 = view::make( root )
                          | view::child( "1" )
                          | view::alias( c2 )
                          | view::single
                          | view::create_node( kmap )
                          | view::to_single;

            THEN( "alias exists" )
            {
                REQUIRE( succ( a1 ) );
            }
            THEN( "alias placed where expected" )
            {
                auto const desc = kmap.fetch_descendant( "/1.2" );

                REQUIRE( succ( desc ) );
                REQUIRE( desc.value() == a1.value() );
            }

            REQUIRE( succ( kmap.erase_node( a1.value() ) ) );
        }
        WHEN( "view::alias( Intermediary ) is used to create an alias" )
        {
            auto const a1 = view::make( root )
                          | view::child( "1" )
                          | view::alias( view::make( root )
                                       | view::child( "2" ) )
                          | view::single
                          | view::create_node( kmap )
                          | view::to_single;

            THEN( "alias exists" )
            {
                REQUIRE( succ( a1 ) );
            }
            THEN( "alias placed where expected" )
            {
                auto const desc = kmap.fetch_descendant( "/1.2" );

                REQUIRE( succ( desc ) );
                REQUIRE( desc.value() == a1.value() );
            }

            REQUIRE( succ( kmap.erase_node( a1.value() ) ) );
        }

        REQUIRE( succ( kmap.erase_node( c2 ) ) );
        REQUIRE( succ( kmap.erase_node( c1 ) ) );
    }
    GIVEN( "kmap blank state" )
    {
        WHEN( "single alias created" )
        {
            auto const c1 = kmap.create_child( root, "1" ); REQUIRE( succ( c1 ) );
            auto const c2 = kmap.create_child( root, "2" ); REQUIRE( succ( c2 ) );
            auto const a1 = kmap.create_alias( c2.value(), c1.value() ); REQUIRE( succ( a1 ) ); // a1 => /1.2

            THEN( "unspecified view::alias finds" )
            {
                auto const v = view::make( root )
                             | view::child( "1" )
                             | view::alias;
                REQUIRE( succ( v | view::fetch_node( kmap ) ) );
                REQUIRE( ( v | view::to_node_set( kmap ) ).size() == 1 );
                REQUIRE( *( v | view::to_node_set( kmap ) ).begin() == a1.value() );
            }
            THEN( "specified view::alias finds" )
            {
                auto const v = view::make( root )
                             | view::child( "1" )
                             | view::alias( "2" );

                REQUIRE( succ( v | view::fetch_node( kmap ) ) );
                REQUIRE( ( v | view::to_node_set( kmap ) ).size() == 1 );
                REQUIRE( *( v | view::to_node_set( kmap ) ).begin() == a1.value() );
            }
            THEN( "nested view::alias finds" )
            {
                auto const v = view::make( root )
                             | view::child( "1" )
                             | view::alias( a1.value() );

                REQUIRE( succ( v | view::fetch_node( kmap ) ) );
                REQUIRE( ( v | view::to_node_set( kmap ) ).size() == 1 );
                REQUIRE( *( v | view::to_node_set( kmap ) ).begin() == a1.value() );
            }

            REQUIRE( succ( kmap.erase_node( a1.value() ) ) );
            REQUIRE( succ( kmap.erase_node( c2.value() ) ) );
            REQUIRE( succ( kmap.erase_node( c1.value() ) ) );
        }
        WHEN( "two aliases exists" )
        {
            // ...
        }
    }
}

SCENARIO( "node_view view::desc", "[path][node_view]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = kmap::Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "single descendant" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "1" ) );
        auto const vr = view::make( root );

        WHEN( "no selection specifier" )
        {
            THEN( "finds exactly one desc" )
            {
                fmt::print( "root id: {}\n", to_string( root ) );
                for( auto const& e : vr
                                   | view::desc
                                   | view::to_node_set( kmap ) )
                {
                    fmt::print( "iter desc: {}\n", to_string( e ) );
                }
                REQUIRE(( vr | view::desc | view::exists( kmap ) ));
                REQUIRE(( vr | view::desc | view::to_node_set( kmap ) ).size() == 1 );
                REQUIRE(( vr | view::desc | view::fetch_node( kmap ) ).value() == c1 );
            }
        }
        WHEN( "<string>" )
        {
            THEN( "indirect" )
            {
                REQUIRE(( vr | view::desc( "1" ) | view::exists( kmap ) ));
                REQUIRE(( vr | view::desc( "1" ) | view::fetch_node( kmap ) ).value() == c1 );
            }
            THEN( "direct" )
            {
                REQUIRE(( vr | view::desc( ".1" ) | view::exists( kmap ) ));
                REQUIRE(( vr | view::desc( ".1" ) | view::fetch_node( kmap ) ).value() == c1 );
            }
            THEN( "parent of desc not found" )
            {
                REQUIRE(!( vr | view::desc( "root" ) | view::exists( kmap ) ));
            }
            THEN( "invalid desc not found" )
            {
                REQUIRE(!( vr | view::desc( "2" ) | view::exists( kmap ) ));
            }
        }
        WHEN( "<uuid>" )
        {
            REQUIRE(( vr | view::desc( c1 ) | view::exists( kmap ) ));
            REQUIRE(( vr | view::desc( c1 ) | view::fetch_node( kmap ) ).value() == c1 );
            REQUIRE(!( vr | view::desc( root ) | view::exists( kmap ) ));
        }
        WHEN( "<Intermediary>" )
        {
            REQUIRE(( vr | view::desc( vr | view::child( "1" ) ) | view::exists( kmap ) ));
            REQUIRE(!( vr | view::desc( vr | view::child( "2" ) ) | view::exists( kmap ) ));
            REQUIRE(!( vr | view::desc( vr | view::child( "root" ) ) | view::exists( kmap ) ));
        }
        WHEN( "<all_of>" )
        {
        }
        WHEN( "<any_of>" )
        {
            REQUIRE(( vr | view::desc( vr | view::child( view::any_of( "1" ) ) ) | view::exists( kmap ) ));
            REQUIRE(( vr | view::desc( vr | view::child( view::any_of( "1", "2" ) ) ) | view::exists( kmap ) ));
            REQUIRE(( vr | view::desc( vr | view::child( view::any_of( "2", "1" ) ) ) | view::exists( kmap ) ));
            REQUIRE(!( vr | view::desc( vr | view::child( view::any_of( "root", "2" ) ) ) | view::exists( kmap ) ));
        }
        WHEN( "<none_of>" )
        {
        }

        REQUIRE( succ( kmap.erase_node( c1 ) ) );
    }
    GIVEN( "/event.[object,verb,subject,outlet]" )
    {
        auto const ver = view::make( kmap.root_node_id() )
                       | view::child( "event" );

        REQUIRE( succ( ver
                     | view::child( view::all_of( "object", "verb", "subject", "outlet" ) )
                     | view::create_node( kmap ) ) );
                    

        THEN( "each child exists" )
        {
            REQUIRE(( ver 
                    | view::child( view::all_of( "object", "verb", "subject", "outlet" ) )
                    | view::exists( kmap ) ));
        }

        // when.. create alias children...
        WHEN( "create outlet structure" )
        {
            auto const alias_srcs = ver | view::child( view::all_of( "object", "verb", "subject" ) );
            
            REQUIRE( succ( ver
                         | view::desc( ".outlet.custom.requisite" )
                         | view::child( view::all_of( "description", "action" ) )
                         | view::create_node( kmap ) ) );
            REQUIRE( succ( ver
                         | view::desc( ".outlet.custom.requisite" )
                         | view::alias( alias_srcs ) 
                         | view::create_node( kmap ) ) );

            THEN( "aliases exist" )
            {
                REQUIRE(( ver 
                        | view::desc( "outlet.custom.requisite" )
                        | view::child( view::all_of( "object", "verb", "subject" ) )
                        | view::exists( kmap ) ));
                REQUIRE(( 3 == ( ver
                               | view::desc( "outlet.custom.requisite" )
                               | view::alias
                               | view::count( kmap ) ) ));
            }
            THEN( "action found" )
            {
                REQUIRE(( ver 
                        | view::child( "outlet" )
                        | view::desc
                        | view::child( view::all_of( "object", "verb", "subject" ) ) // TODO: Rather alias()... and ensure aliases are from object/subject/verb?
                        | view::parent
                        | view::child( "action" )
                        | view::exists( kmap ) ));
            }
            
            REQUIRE( succ( ver
                         | view::desc( ".outlet.custom.requisite" )
                         | view::alias( alias_srcs ) 
                         | view::erase_node( kmap ) ) );
            REQUIRE( succ( ver
                         | view::child( view::all_of( "description", "action" ) )
                         | view::erase_node( kmap ) ) );
        }

        REQUIRE( succ( ver
                     | view::erase_node( kmap ) ) );
    }
}

SCENARIO( "node_view view::create_node", "[path][node_view]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = kmap::Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "root" )
    {
        WHEN( "create view::desc" )
        {
            REQUIRE( fail( view::make( root )
                         | view::desc( "1" ) // Some "decendant" isn't specific enough for node creation.
                         | view::create_node( kmap ) ) );
            REQUIRE_RES( view::make( root )
                       | view::desc( ".1" )
                       | view::create_node( kmap ) );
        }
    }
}

SCENARIO( "node_view view::single", "[path][node_view]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = kmap::Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "ambiguous 'charlie'" )
    {
        REQUIRE( succ( view::make( root )
                     | view::direct_desc( "victor.charlie" ) 
                     | view::single
                     | view::create_node( kmap )
                     | view::to_single ) );
        REQUIRE( succ( view::make( root )
                     | view::direct_desc( "delta.charlie" ) 
                     | view::single
                     | view::create_node( kmap )
                     | view::to_single ) );

        WHEN( "fetching unique on ambiguous 'charlie'" )
        {
            auto const v = view::make( root )
                         | view::desc( "charlie" )
                         | view::single;

            REQUIRE( ( v | view::to_node_set( kmap ) ).empty() );
            REQUIRE( fail( v | view::fetch_node( kmap ) ) );
            
        }
        WHEN( "fetching unique on disambiguated 'charlie'" )
        {
            auto const v = view::make( root )
                         | view::direct_desc( "delta.charlie" )
                         | view::single;

            REQUIRE( ( v | view::to_node_set( kmap ) ).size() == 1 );
            REQUIRE( succ( v | view::fetch_node( kmap ) ) );
        }

        REQUIRE( succ( view::make( root )
                     | view::direct_desc( "victor.charlie" ) 
                     | view::erase_node( kmap ) ) );
        REQUIRE( succ( view::make( root )
                     | view::direct_desc( "delta.charlie" ) 
                     | view::erase_node( kmap ) ) );
    }
}

SCENARIO( "node_view view::erase", "[path][node_view]" )
{
    KMAP_BLANK_STATE_FIXTURE_SCOPED();

    auto& kmap = kmap::Singleton::instance();
    auto const root = kmap.root_node_id();

    GIVEN( "two siblings" )
    {
        auto const c1 = REQUIRE_TRY( kmap.create_child( root, "c1" ) );
        auto const c2 = REQUIRE_TRY( kmap.create_child( root, "c2" ) );

        WHEN( "siblings erased" )
        {
            REQUIRE( succ( view::make( root ) | view::child | view::erase_node( kmap ) ) );

            THEN( "siblings don't exist" )
            {
                REQUIRE( !( view::make( c1 ) | view::exists( kmap ) ) );
                REQUIRE( !( view::make( c2 ) | view::exists( kmap ) ) );
            }

            WHEN( "child is created" )
            {
                auto const c3 = REQUIRE_TRY( kmap.create_child( root, "c3" ) );

                THEN( "child exists" )
                {
                    REQUIRE( view::make( c3 ) | view::exists( kmap ) );
                }
                THEN( "child is ordered" )
                {
                    REQUIRE_NOTHROW( kmap.fetch_children_ordered( root ) );
                    auto const ordered = kmap.fetch_children_ordered( root );

                    REQUIRE( ordered == std::vector{ c3 } );
                }
            }
        }
    }
}

namespace utf = boost::unit_test;

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( kmap_iface )
BOOST_AUTO_TEST_SUITE( /*kmap_iface*/node_view
                     , 
                     * utf::depends_on( "kmap_iface/create_node" ) )

auto to_headings( Kmap const& kmap
                , auto const& rng )
{
    return rng
         | ranges::views::transform( [ & ]( auto const& e ){ return kmap.fetch_heading( e ).value(); } )
         | ranges::to< std::set< std::string > >();
};

BOOST_AUTO_TEST_CASE( misc
                    ,
                    * utf::fixture< ClearMapFixture >() )
{
    using namespace std::string_literals;

    auto& kmap = Singleton::instance();

    auto nodes = create_lineages( "/test"
                                , "/test.1.2"
                                , "/test.3" );
    auto const root = nodes[ "/test" ];

    // Children of root
    {
        auto const intm = view::make( root )
                        | view::child
                        | view::to_node_set( kmap );
        
        BOOST_TEST(( to_headings( kmap, intm ) == std::set< std::string >{ "1", "3" } ));
    }
    // All but root
    {
        auto const intm = view::make( root )
                        | view::desc
                        | view::to_node_set (kmap );
        
        BOOST_TEST(( to_headings( kmap, intm ) == std::set< std::string >{ "1", "2", "3" } ));
    }
    // All children of root desc who have children
    {
        auto const intm = view::make( root )
                        | view::desc
                        | view::child
                        | view::to_node_set( kmap );
        
        BOOST_TEST(( to_headings( kmap, intm ) == std::set< std::string >{ "2" } ));
    }
    // All Desc "1"
    {
        auto const intm = view::make( root )
                        | view::desc( "1"s )
                        | view::to_node_set( kmap );
        
        BOOST_TEST(( to_headings( kmap, intm ) == std::set< std::string >{ "1" } ));
    }
    // All Desc "1", "3"
    {
        auto const intm = view::make( root )
                        | view::desc( view::any_of( "1", "3" ) )
                        | view::to_node_set( kmap );
        
        BOOST_TEST(( to_headings( kmap, intm ) == std::set< std::string >{ "1", "3" } ));
    }
    // None Desc "1", "3"
    {
        auto const intm = view::make( root )
                        | view::desc( view::none_of( "1", "3" ) )
                        | view::to_node_set( kmap );
        
        BOOST_TEST(( to_headings( kmap, intm ) == std::set< std::string >{ "2" } ));
    }
    // Desc of "test" is "1.2"
    {
        auto const intm = view::make( root )
                        | view::desc( "1.2" ) // So, when treating Heading <=> HeadingPath, does this still function as expected?
                        | view::to_node_set( kmap );
        
        BOOST_TEST(( to_headings( kmap, intm ) == std::set< std::string >{ "2" } ));
    }
}

BOOST_AUTO_TEST_SUITE_END( /* kmap_iface/node_view */ )
BOOST_AUTO_TEST_SUITE_END( /* kmap_iface */ )

} // namespace kmap::test