/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../db.hpp"
#include "test/master.hpp"
#include "test/util.hpp"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/iterator/operations.hpp>

#include <iterator>

namespace fs = boost::filesystem;
namespace utf = boost::unit_test;
using namespace kmap;
using namespace kmap::db;

SCENARIO( "DB basics manipulated", "[db]" )
{
    auto db = Database{};
    
    GIVEN( "db is empty" )
    {
        THEN( "fail to fetch node" )
        {
            REQUIRE( fail( db.node_exists( gen_uuid() ) ) );
        }
        THEN( "fail to erase node" )
        {
            REQUIRE( fail( db.erase< NodeTable >( gen_uuid() ) ) );
        }

        WHEN( "push node" )
        {
            auto const n1 = gen_uuid();

            REQUIRE( succ( db.push_node( n1 ) ) );

            THEN( "fetch" )
            {
                REQUIRE( succ( db.node_exists( n1 ) ) );
            }
        }
    }
    GIVEN( "db has node entry" )
    {
        auto const n1 = gen_uuid();

        REQUIRE( succ( db.push_node( n1 ) ) );

        THEN( "fetch node" )
        {
            REQUIRE( succ( db.node_exists( n1 ) ) );
        }

        WHEN( "node is erased" )
        {
            REQUIRE( succ( db.erase< NodeTable >( n1 ) ) );
            
            THEN( "fail to fetch" )
            {
                REQUIRE( fail( db.node_exists( n1 ) ) );
            }
        }
        WHEN( "all tables are erased" )
        {
            db.erase_all( n1 );

            THEN( "fail to fetch node" )
            {
                REQUIRE( fail( db.node_exists( n1 ) ) );
            }
        }
    }
    GIVEN( "db has parent-child nodes" )
    {
        auto const n1 = gen_uuid();
        auto const n2 = gen_uuid();

        REQUIRE( succ( db.push_node( n1 ) ) );
        REQUIRE( succ( db.push_node( n2 ) ) );

        REQUIRE( succ( db.push_child( n1, n2 ) ) );

        THEN( "fetch parent" )
        {
            REQUIRE( succ( db.fetch_parent( n2 ) ) );
        }
        THEN( "fetch child" )
        {
            REQUIRE( succ( db.fetch_children( n1 ) ) );
            REQUIRE( db.fetch_children( n1 ).value().size() == 1 );
            REQUIRE( db.fetch_children( n1 ).value().contains( n2 ) );
        }
        THEN( "child has no children" )
        {
            REQUIRE( succ( db.fetch_children( n2 ) ) ); // TODO: Should it succeed even if empty?
            REQUIRE( db.fetch_children( n2 ).value().empty() );
        }

        WHEN( "child is deleted" ) 
        {
            REQUIRE( succ( db.erase< ChildTable >( ChildTable::unique_key_type{ Parent{ n1 }, Child{ n2 } } ) ) );

            THEN( "fail fetch" )
            {
                REQUIRE( succ( db.fetch_children( n1 ) ) );
                REQUIRE( db.fetch_children( n1 ).value().empty() );
            }
        }
    }
}

SCENARIO( "db interacts with disk", "[db][env]" )
{
    auto db = Database{};

    KMAP_INIT_DISK_DB_FIXTURE_SCOPED( db );

    GIVEN( "set table entry" )
    {
        auto const nid = gen_uuid();
        
        REQUIRE( succ( db.push_node( nid ) ) );

        auto const& table = db.cache().fetch< db::NodeTable >();

        REQUIRE( ranges::distance( table ) == 1 );

        THEN( "delta item exists" )
        {
            auto const& t_item = *table.begin();

            REQUIRE( t_item.delta_items.size() == 1 );
            REQUIRE( t_item.delta_items.back().value == nid );
        }
        THEN( "cache item does not exist" )
        {
            auto const& t_item = *table.begin();

            REQUIRE( !t_item.cache_item );
        }

        WHEN( "flush to delta" )
        {
            REQUIRE( succ( db.flush_delta_to_disk() ) );

            THEN( "table item still exists" )
            {
                REQUIRE( ranges::distance( table ) == 1 );
            }
            THEN( "delta item does not exist" )
            {
                auto const& t_item = *table.begin();

                REQUIRE( t_item.delta_items.size() == 0 );
            }
            THEN( "cache item exists" )
            {
                auto const& t_item = *table.begin();

                REQUIRE( t_item.cache_item );
                REQUIRE( t_item.cache_item.value() == nid );
            }
            THEN( "db.fetch still finds item" )
            {
                REQUIRE( db.node_exists( nid ) );
            }
            THEN( "item exists on disk" )
            {
                auto const nt = nodes::nodes{};
                {
                    auto rows = db.execute( select( all_of( nt ) )
                                          . from( nt )
                                          . unconditionally() );
                    REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                }
                {
                    auto rows = db.execute( select( all_of( nt ) )
                                          . from( nt )
                                          . unconditionally() );
                    REQUIRE( rows.begin()->uuid == to_string( nid ) );
                }
            }
        }
        WHEN( "item erased" )
        {
            REQUIRE_NOTHROW( db.erase_all( nid ) );

            WHEN( "delta flushed" )
            {
                REQUIRE( succ( db.flush_delta_to_disk() ) );

                THEN( "table item gone" )
                {
                    REQUIRE( ranges::distance( table ) == 0 );
                }
                THEN( "db.fetch does not find item" )
                {
                    REQUIRE( !db.node_exists( nid ) );
                }
                THEN( "item does not exists on disk" )
                {
                    auto const nt = nodes::nodes{};
                    {
                        auto rows = db.execute( select( all_of( nt ) )
                                              . from( nt )
                                              . unconditionally() );
                        REQUIRE( std::distance( rows.begin(), rows.end() ) == 0 );
                    }
                }
                THEN( "item recreation succeeds" )
                {
                    REQUIRE( succ( db.push_node( nid ) ) );
                }
            }
        }
    }
    GIVEN( "L table entry" )
    {
        auto const nid = gen_uuid();
        auto const h1 = "h1";
        
        REQUIRE( succ( db.push_node( nid ) ) );
        REQUIRE( succ( db.push_heading( nid, h1 ) ) );
        fmt::print( "push_heading( {} )\n", to_string( nid ) );

        auto const& table = db.cache().fetch< db::HeadingTable >();

        REQUIRE( ranges::distance( table ) == 1 );

        THEN( "delta item exists" )
        {
            auto const& t_item = *table.begin();

            REQUIRE( t_item.delta_items.size() == 1 );
            REQUIRE( t_item.delta_items.back().value == h1 );
        }
        THEN( "cache item does not exist" )
        {
            auto const& t_item = *table.begin();

            REQUIRE( !t_item.cache_item );
        }

        WHEN( "flush to delta" )
        {
            REQUIRE( succ( db.flush_delta_to_disk() ) );

            THEN( "table item still exists" )
            {
                REQUIRE( ranges::distance( table ) == 1 );
            }
            THEN( "delta item does not exist" )
            {
                auto const& t_item = *table.begin();

                REQUIRE( t_item.delta_items.size() == 0 );
            }
            THEN( "cache item exists" )
            {
                auto const& t_item = *table.begin();

                REQUIRE( t_item.cache_item );
                REQUIRE( t_item.cache_item.value() == h1 );
            }
            THEN( "db.fetch still finds item" )
            {
                REQUIRE( db.contains< db::HeadingTable >( nid ) );
            }
            THEN( "item exists on disk" )
            {
                auto const ht = headings::headings{};
                {
                    auto rows = db.execute( select( all_of( ht ) )
                                          . from( ht )
                                          . unconditionally() );
                    REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                }
                {
                    auto rows = db.execute( select( all_of( ht ) )
                                          . from( ht )
                                          . unconditionally() );
                    REQUIRE( rows.begin()->heading == h1 );
                }
            }
            THEN( "item update succeeds" )
            {
                auto const h2 = "h2";
                REQUIRE( succ( db.push_heading( nid, h2 ) ) );
            }
        }

        WHEN( "item modified" )
        {
            auto const h2 = "h2";
            
            fmt::print( "update_heading( {} )\n", to_string( nid ) );
            REQUIRE( succ( db.update_heading( nid, h2 ) ) );

            THEN( "table item still exists" )
            {
                REQUIRE( ranges::distance( table ) == 1 );
            }
            THEN( "delta now has two items" )
            {
                auto const& t_item = *table.begin();

                REQUIRE( t_item.delta_items.size() == 2 );
            }

            WHEN( "delta flushed" )
            {
                REQUIRE( succ( db.flush_delta_to_disk() ) );

                THEN( "table item still exists" )
                {
                    REQUIRE( ranges::distance( table ) == 1 );
                }
                THEN( "delta item does not exist" )
                {
                    auto const& t_item = *table.begin();

                    REQUIRE( t_item.delta_items.size() == 0 );
                }
                THEN( "cache item exists" )
                {
                    auto const& t_item = *table.begin();

                    REQUIRE( t_item.cache_item );
                    REQUIRE( t_item.cache_item.value() == h2 );
                }
                THEN( "db.fetch still finds item" )
                {
                    REQUIRE( db.contains< db::HeadingTable >( nid ) );
                }
                THEN( "item exists on disk" )
                {
                    auto const ht = headings::headings{};
                    {
                        auto rows = db.execute( select( all_of( ht ) )
                                            . from( ht )
                                            . unconditionally() );
                        REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                    }
                    {
                        auto rows = db.execute( select( all_of( ht ) )
                                            . from( ht )
                                            . unconditionally() );
                        REQUIRE( rows.begin()->heading == h2 );
                    }
                }
            }
        }

        WHEN( "item erased" )
        {
            REQUIRE_NOTHROW( db.erase_all( nid ) );
            REQUIRE( ranges::distance( table ) == 1 ); // Erase as a delta, so table item remains.
            fmt::print( "db.erase_all()\n" );

            WHEN( "delta flushed" )
            {
                REQUIRE( succ( db.flush_delta_to_disk() ) );
                fmt::print( "db.flush_delta_to_disk()\n" );

                THEN( "table item gone" )
                {
                    REQUIRE( ranges::distance( table ) == 0 );
                }
                THEN( "db.fetch does not find item" )
                {
                    REQUIRE( !db.node_exists( nid ) );
                }
                THEN( "item does not exists on disk" )
                {
                    auto const nt = nodes::nodes{};
                    {
                        auto rows = db.execute( select( all_of( nt ) )
                                              . from( nt )
                                              . unconditionally() );
                        REQUIRE( std::distance( rows.begin(), rows.end() ) == 0 );
                    }
                }
                THEN( "item recreation succeeds" )
                {
                    REQUIRE( succ( db.push_node( nid ) ) );
                    REQUIRE( succ( db.push_heading( nid, h1 ) ) );
                }
            }
        }
    }
    GIVEN( "LR table entry" )
    {
        auto const n1 = gen_uuid();
        auto const n2 = gen_uuid();
        
        REQUIRE( succ( db.push_node( n1 ) ) );
        REQUIRE( succ( db.push_node( n2 ) ) ); // TODO: think this one is necessary...
        REQUIRE( succ( db.push_child( n1, n2 ) ) );

        auto const& table = db.cache().fetch< db::ChildTable >();

        REQUIRE( ranges::distance( table ) == 1 );

        THEN( "delta item exists" )
        {
            auto const& t_item = *table.begin();

            REQUIRE( t_item.delta_items.size() == 1 );
            REQUIRE( t_item.delta_items.back().value.second.value() == n2 );
        }
        THEN( "cache item does not exist" )
        {
            auto const& t_item = *table.begin();

            REQUIRE( !t_item.cache_item );
        }

        WHEN( "flush to disk" )
        {
            REQUIRE( succ( db.flush_delta_to_disk() ) );

            THEN( "table item still exists" )
            {
                REQUIRE( ranges::distance( table ) == 1 );
            }
            THEN( "delta item does not exist" )
            {
                auto const& t_item = *table.begin();

                REQUIRE( t_item.delta_items.size() == 0 );
            }
            THEN( "cache item exists" )
            {
                auto const& t_item = *table.begin();

                REQUIRE( t_item.cache_item );
                REQUIRE( t_item.cache_item.value().second.value() == n2 );
            }
            THEN( "db.fetch still finds item" )
            {
                REQUIRE( db.contains< db::ChildTable >( db::Right{ n2 } ) );
            }
            THEN( "item exists on disk" )
            {
                auto const ct = children::children{};
                {
                    auto rows = db.execute( select( all_of( ct ) )
                                          . from( ct )
                                          . unconditionally() );
                    REQUIRE( std::distance( rows.begin(), rows.end() ) == 1 );
                }
                {
                    auto rows = db.execute( select( all_of( ct ) )
                                          . from( ct )
                                          . unconditionally() );
                    REQUIRE( rows.begin()->child_uuid == to_string( n2 ) );
                }
            }
        }

        WHEN( "item erased" )
        {
            REQUIRE_NOTHROW( db.erase_all( n2 ) );
            REQUIRE( ranges::distance( table ) == 1 ); // Erase as a delta, so table item remains.

            WHEN( "delta flushed" )
            {
                REQUIRE( succ( db.flush_delta_to_disk() ) );

                THEN( "table item gone" )
                {
                    REQUIRE( ranges::distance( table ) == 0 );
                }
                THEN( "db.fetch does not find item" )
                {
                    REQUIRE( !db.node_exists( n2 ) );
                }
                THEN( "item does not exists on disk" )
                {
                    auto const ct = children::children{};
                    {
                        auto rows = db.execute( select( all_of( ct ) )
                                              . from( ct )
                                              . unconditionally() );
                        REQUIRE( std::distance( rows.begin(), rows.end() ) == 0 );
                    }
                }
            }
        }
    }
    GIVEN( "parent, child" )
    {
        auto const n1 = gen_uuid();
        auto const n2 = gen_uuid();

        REQUIRE( succ( db.push_node( n1 ) ) );
        REQUIRE( succ( db.push_node( n2 ) ) );
        REQUIRE( succ( db.push_child( n1, n2 ) ) );

        THEN( "fetch parent" )
        {
            REQUIRE( succ( db.fetch_parent( n2 ) ) );
            REQUIRE( fail( db.fetch_parent( n1 ) ) );
        }

        WHEN( "delta flushed" )
        {
            REQUIRE( succ( db.flush_delta_to_disk() ) );
            fmt::print( "flush to delta\n" );

            WHEN( "sibling created" )
            {
                auto const n3 = gen_uuid();

                REQUIRE( succ( db.push_node( n3 ) ) );
                REQUIRE( succ( db.push_child( n1, n3 ) ) );
                fmt::print( "created sibling\n" );

                THEN( "fetch parent" )
                {
                    fmt::print( "n1: {}\n", to_string( n1 ) );
                    fmt::print( "n2: {}\n", to_string( n2 ) );
                    fmt::print( "n3: {}\n", to_string( n3 ) );
                    REQUIRE( succ( db.fetch_parent( n3 ) ) );
                    REQUIRE( succ( db.fetch_parent( n2 ) ) );
                    REQUIRE( fail( db.fetch_parent( n1 ) ) );
                }
            }
        }
    }
}

namespace kmap::test {

BOOST_AUTO_TEST_SUITE( database )
                    //  , 
                    //  * utf::depends_on( "cache" ) ) // TODO: Actually, strictly speaking, only on a "save" or "write to disk" is FS required. But, of course, 
                    //  * utf::depends_on( "filesystem" ) ) // TODO: Actually, strictly speaking, only on a "save" or "write to disk" is FS required. But, of course, 
                    //  * utf::depends_on( "sqlpp11" ) )
                      
BOOST_AUTO_TEST_CASE( /*database*/insert )
{
    // auto const tmp_db_name = fs::path{ fmt::format( "{}.kmap", to_string( gen_uuid() ) ) };

    // BOOST_REQUIRE( !fs::exists( tmp_db_name ) );

    {
        auto db = Database{};

        auto const pid = gen_uuid();
        auto const ph = "echo";
        auto const pb = "charlie";

        BOOST_REQUIRE( db.push_node( pid ).has_value() );
        BOOST_REQUIRE( db.push_heading( pid, ph ).has_value() );
        BOOST_REQUIRE( db.push_body( pid, pb ).has_value() );

        BOOST_REQUIRE( db.node_exists( pid ) );
        BOOST_REQUIRE( db.fetch_heading( pid ).has_value() );
        BOOST_REQUIRE( db.fetch_body( pid ).has_value() );

        BOOST_TEST( db.fetch_heading( pid ).value() == ph );
        BOOST_TEST( db.fetch_body( pid ).value() == pb );

        auto const cid = gen_uuid();
        auto const ch = "alpha";
        auto const cb = "delta";

        BOOST_REQUIRE( db.push_node( cid ).has_value() );
        BOOST_REQUIRE( db.push_heading( cid, ch ).has_value() );
        BOOST_REQUIRE( db.push_body( cid, cb ).has_value() );
        BOOST_REQUIRE( db.push_child( pid, cid ).has_value() );

        BOOST_REQUIRE( db.node_exists( cid ) );
        BOOST_REQUIRE( db.fetch_heading( cid ).has_value() );
        BOOST_REQUIRE( db.fetch_body( cid ).has_value() );
        BOOST_REQUIRE( db.fetch_children( pid ).has_value() );
        BOOST_REQUIRE( db.fetch_children( pid ).value().size() == 1 );
        BOOST_REQUIRE( db.fetch_parent( cid ).has_value() );
        BOOST_REQUIRE( db.fetch_child( pid, ch ).has_value() );

        BOOST_TEST( db.fetch_heading( cid ).value() == ch );
        BOOST_TEST( db.fetch_body( cid ).value() == cb );
        BOOST_TEST( db.fetch_parent( cid ).value() == pid );
        BOOST_TEST( db.fetch_children( pid ).value().contains( cid ) );
        BOOST_REQUIRE( db.fetch_child( pid, ch ).value() == cid );
    }
}

BOOST_AUTO_TEST_CASE( /*database*/update )
{
}

BOOST_AUTO_TEST_SUITE_END( /* database */ )

} // namespace kmap::test