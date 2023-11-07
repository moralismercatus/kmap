/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/network/network.hpp"

#include "attribute.hpp"
#include "com/database/db.hpp"
#include "com/database/root_node.hpp"
#include "com/visnetwork/visnetwork.hpp"
#include "error/result.hpp"
#include "error/result.hpp"
#include "kmap.hpp"
#include "path/act/abs_path.hpp"
#include "path/act/fetch_heading.hpp"
#include "path/act/order.hpp"
#include "path/act/take.hpp"
#include "path/node_view2.hpp"
#include "test/util.hpp"
#include "utility.hpp"
#include "util/result.hpp"

#include <boost/json.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/action/join.hpp>
#include <range/v3/action/reverse.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/replace.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_last.hpp>

using namespace ranges;
namespace rvs = ranges::views;
namespace ras = ranges::actions;

namespace kmap::com {

Network::Network( Kmap& km
                , std::set< std::string > const& requisites
                , std::string const& description )
    : Component{ km, requisites, description }
{
}

auto Network::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const rn = KTRY( fetch_component< com::RootNode >() );

    selected_node_ = rn->root_node();
    
    rv = outcome::success();

    return rv;
}

auto Network::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const rn = KTRY( fetch_component< com::RootNode >() );

    selected_node_ = rn->root_node();

    KTRY( load_aliases() );

    rv = outcome::success();

    return rv;
}

auto Network::load_aliases()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const db = KTRY( fetch_component< com::Database >() );
    
    auto const alias_tbl = db->fetch< db::AliasTable >();
    auto all_aliases = alias_tbl
                     | rvs::transform([]( auto const& e ){ return AliasLoadItem{ AliasItem{ .src_id = AliasItem::src_type{ e.left().value( ) }
                                                                                          , .rsrc_id = AliasItem::rsrc_type{ e.left().value() }
                                                                                          , .dst_id = AliasItem::dst_type{ e.right().value() } } }; } )
                     | ranges::to< AliasLoadSet >();

    // auto& av = all_aliases.get< AliasLoadItem::loaded_type >();
    auto eq = all_aliases.get< AliasLoadItem::loaded_type >().equal_range( AliasLoadItem::loaded_type{ false } );

    while( eq.first != eq.second )
    {
        auto const next = *eq.first;

        KTRY( load_alias_leaf( next.src().value(), next.dst().value(), all_aliases ) );

        eq = all_aliases.get< AliasLoadItem::loaded_type >().equal_range( AliasLoadItem::loaded_type{ false } );
    }

    rv = outcome::success();

    return rv;
}

auto Network::alias_store()
    -> AliasStore&
{
    return astore_;
}

auto Network::alias_store() const
    -> AliasStore const&
{
    return astore_;
}

auto Network::copy_body( Uuid const& src
                       , Uuid const& dst )
    -> Result< void >
{
    return kmap::copy_body( *this, src, dst );
}

// TODO: This fails if alias source is "/" - add a TC for this.
// TODO: Test when src is child of dst
auto Network::create_alias( Uuid const& src
                          , Uuid const& dst )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "src", src );
        // KM_RESULT_PUSH_NODE( "dst", dst );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const rsrc = resolve( src );
    auto const rdst = resolve( dst );
    auto const alias_id = make_alias_id( rsrc, rdst );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );
                BC_ASSERT( fetch_parent( rv.value() ).has_value() );
                BC_ASSERT( is_top_alias( rv.value() ) );

                {
                    auto const src_cnt = view::make( src )
                                       | view::desc 
                                       | view::count( km );
                    auto const rv_cnt = view::make( rv.value() )
                                      | view::desc
                                      | view::count( km );
                    BC_ASSERT( src_cnt == rv_cnt );

                    {
                        auto const db = KTRYE( km.fetch_component< com::Database >() );
                        BC_ASSERT( db->alias_exists( rsrc, rdst ) );
                    }
                }
            }
        })
    ;

    KMAP_ENSURE( src != km.root_node_id(), error_code::network::invalid_node );
    KMAP_ENSURE( exists( rsrc ), error_code::create_alias::src_not_found );
    KMAP_ENSURE( exists( rdst ), error_code::create_alias::dst_not_found );
    KMAP_ENSURE( rsrc != rdst, error_code::create_alias::src_equals_dst );
    KMAP_ENSURE( !is_lineal( rsrc, rdst ), error_code::create_alias::src_ancestor_of_dst );
    KMAP_ENSURE_MSG( !exists( alias_id ), error_code::create_alias::alias_already_exists, KTRYE( absolute_path_flat( km, rsrc ) ) );
    KMAP_ENSURE_MSG( !is_child( rdst, KTRYE( fetch_heading( rsrc ) ) ), error_code::create_node::duplicate_child_heading, KTRYE( fetch_heading( rsrc ) ) );
    KMAP_ENSURE( !is_child( rdst, rsrc ), error_code::network::invalid_node );
    KMAP_ENSURE( !is_alias( alias_id ), error_code::network::invalid_node );

    {
        auto const db = KTRY( km.fetch_component< com::Database >() );

        KTRY( db->push_alias( rsrc, rdst ) );
    }
    
    if( !attr::is_in_attr_tree( km, dst ) )
    {
        // TODO: Replace with event/outlet.
        KTRY( attr::push_order( km, rdst, rsrc ) ); // Resolve src ID gets placed in ordering, rather than the alias ID.
    }

    auto const pushed_id = KTRY( create_alias_leaf( rsrc, rdst ) );

    for( auto const& alias : astore_.fetch_aliases( AliasItem::rsrc_type{ rdst } ) )
    {
        KTRY( create_alias_leaf( rsrc, alias ) );
    }

    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        KTRY( estore.value()->fire_event( { "subject.network", "verb.created", "object.alias" }, { { "alias_id", id } } ) );
    }

    rv = pushed_id;

    return rv;
}

SCENARIO( "Network::create_alias", "[network][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const rnc = REQUIRE_TRY( km.fetch_component< com::RootNode >() );
    auto const rootn = rnc->root_node();

    GIVEN( "/1, /2" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( rootn, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( rootn, "2" ) );

        GIVEN( "create_alias( src:/1, dst:/2 )" )
        {
            auto const a2_1 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

            THEN( "/2.1[/1] is alias" )
            {
                REQUIRE( nw->is_alias( a2_1 ) );
                REQUIRE( nw->is_top_alias( a2_1 ) );
                REQUIRE(( anchor::node( n2 )
                        | view2::alias( view2::resolve( n1 ) )
                        | act2::exists( km ) ));
            }

            GIVEN( "/1.1" )
            {
                auto const n1_1 = REQUIRE_TRY( nw->create_child( n1, "1" ) );

                THEN( "/2.1.1" )
                {
                    auto const a2_1_1 = REQUIRE_TRY(( anchor::node( n2 )
                                                    | view2::alias( view2::resolve( n1 ) )
                                                    | view2::alias( view2::resolve( n1_1 ) )
                                                    | act2::fetch_node( km ) ));

                    REQUIRE( nw->resolve( n1_1 ) == nw->resolve( a2_1_1 ) );
                }

                GIVEN( "/3" )
                {
                    auto const n3 = REQUIRE_TRY( nw->create_child( rootn, "3" ) );

                    GIVEN( "create_alias( src:/3, dst:/1.1 )" )
                    {
                        auto const a1_1_3 = REQUIRE_TRY( nw->create_alias( n3, n1_1 ) );

                        REQUIRE(( anchor::node( n1_1 )
                                | view2::alias( view2::resolve( n3 ) )
                                | act2::exists( km ) ));

                        REQUIRE( nw->resolve( a1_1_3 ) == n3 );

                        THEN( "/2.1[/1].1[/1.1].3[/3] exists" )
                        {
                            REQUIRE(( anchor::node( n2 )
                                    | view2::alias( view2::resolve( n1 ) )
                                    | view2::alias( view2::resolve( n1_1 ) )
                                    | view2::alias( view2::resolve( n3 ) )
                                    | act2::exists( km ) ));
                        }
                    }

                    GIVEN( "create_alias( src:/3, dst:/2.1.1 )" )
                    {
                        auto const a2_1_1 = REQUIRE_TRY(( anchor::node( n2 )
                                                        | view2::alias( view2::resolve( n1 ) )
                                                        | view2::alias( view2::resolve( n1_1 ) )
                                                        | act2::fetch_node( km ) ));
                        auto const a2_1_1_3 = REQUIRE_TRY( nw->create_alias( n3, a2_1_1 ) );

                        REQUIRE( nw->resolve( a2_1_1_3 ) == n3 );

                        THEN( "/2.1[/1].1[/1.1].3[/3] exists" )
                        {
                            REQUIRE(( anchor::node( n2 )
                                    | view2::alias( view2::resolve( n1 ) )
                                    | view2::alias( view2::resolve( n1_1 ) )
                                    | view2::alias( view2::resolve( n3 ) )
                                    | act2::exists( km ) ));
                        }
                    }
                }
            }
        }
    }
}

SCENARIO( "create_alias on attribute", "[network][alias][attribute]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );

    GIVEN( "alias source" )
    {
        auto const tag = REQUIRE_TRY( anchor::abs_root
                                    | view2::child( "tag" )
                                    | act2::create_node( km ) );

        GIVEN( "/1" )
        {
            auto const n1 = REQUIRE_TRY( anchor::abs_root
                                    | view2::child( "1" )
                                    | act2::create_node( km ) );
            GIVEN( "/1.$.t" )
            {
                auto const attrn = REQUIRE_TRY( anchor::node( n1 )
                                            | view2::attr
                                            | view2::child( "t" )
                                            | act2::create_node( km ) );

                WHEN( "alias: /1.$.t.<alias:/tag>" )
                {
                    REQUIRE_TRY( nw->create_alias( tag, attrn ) );

                    THEN( "no node in /1.$ tree has an attribute itself" )
                    {
                        for( auto const& att : anchor::node( attrn )
                                                | view2::desc( view2::none_of( view2::alias ) )
                                                | view2::attr
                                                | act2::to_node_set( km ) )
                        {
                            fmt::print( "ATTRIBUTE OF ATTRIBUTE: {}\n", att );
                        }

                        REQUIRE( !( anchor::node( attrn )
                                  | view2::desc( view2::none_of( view2::alias ) )
                                  | view2::attr
                                  | act2::exists( km ) ) );
                    }
                }
            }
        } 
    }
}

auto Network::create_child( Uuid const& parent
                          , Uuid const& child
                          , Heading const& heading
                          , Title const& title )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        // KM_RESULT_PUSH_NODE( "child", child );
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !alias_store().is_alias( child ) );
            BC_ASSERT( !attr::is_in_order( km, parent, child ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                auto const& rvv = rv.value();

                BC_ASSERT( exists( rvv ) );
                BC_ASSERT( is_child( parent, heading ) );
                BC_ASSERT( is_valid_heading( KTRYE( fetch_heading( rvv ) ) ) );
                BC_ASSERT( KTRYE( fetch_parent( rvv ) ) == resolve( parent ) );

                if( !attr::is_in_attr_tree( km, child ) )
                {
                    BC_ASSERT( attr::is_in_order( km, parent, child ) );
                    // TODO: BC_ASSERT( attr::has_genesis( *this, child ) ); 
                }
            }
        })
    ;

    auto const db = KTRY( fetch_component< com::Database >() );

    KMAP_ENSURE( is_valid_heading( heading ), error_code::node::invalid_heading );
    KMAP_ENSURE( db->node_exists( alias_store().resolve( parent ) ), error_code::create_node::invalid_parent );
    KMAP_ENSURE( !exists( child ), error_code::create_node::node_already_exists );
    KMAP_ENSURE_MSG( !is_child( parent, heading ), error_code::create_node::duplicate_child_heading, fmt::format( "{}:{}", absolute_path_flat( km, parent ), heading ) );

    auto const rparent = alias_store().resolve( parent );

    KTRY( create_child_internal( rparent, child, heading, title ) );

    if( !attr::is_in_attr_tree( km, child ) )
    {
        KTRY( attr::push_genesis( km, child ) );
        KTRY( attr::push_order( km, rparent, child ) );
    }

    KTRY( create_child_aliases( rparent, child ) );

    rv = child;

    return rv;
}

auto Network::create_child( Uuid const& parent
                          , Uuid const& child
                          , Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_NODE( "child", child );
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( create_child( parent
                           , child
                           , heading
                           , format_title( heading ) ) );

    return rv;
}

auto Network::create_child( Uuid const& parent 
                          , Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "parent", parent );
        KM_RESULT_PUSH( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( create_child( parent
                           , gen_uuid()
                           , heading ) );

    return rv;
}

auto Network::create_child( Uuid const& parent 
                          , Heading const& heading
                          , Title const& title )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "parent", parent );
        KM_RESULT_PUSH( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( create_child( parent
                           , gen_uuid()
                           , heading
                           , title ) );

    return rv;
}

auto Network::create_alias_leaf( Uuid const& src
                               , Uuid const& dst )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src", src );
        KM_RESULT_PUSH( "dst", dst );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const rsrc = resolve( src );
    auto const alias_item = make_alias_item( src, rsrc, dst );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_parent( alias_item.alias() ).has_value() );
                BC_ASSERT( fetch_parent( alias_item.alias() ).value() == dst );
            }
        })
    ;

    KMAP_ENSURE( exists( rsrc ), error_code::network::invalid_node );
    KMAP_ENSURE( exists( dst ), error_code::network::invalid_node );

    auto const alias_id = KTRY( astore_.push_alias( alias_item ) );

    for( auto const& e : fetch_children( rsrc ) )
    {
        KTRY( create_alias_leaf( e, alias_id ) );
    }

    rv = alias_id;

    return rv;
}

auto Network::create_child_internal( Uuid const& parent
                                   , Uuid const& child
                                   , Heading const& heading
                                   , Title const& title )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_NODE( "child", child );
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const db = KTRY( fetch_component< com::Database >() );

    KTRY( db->push_node( child ) );
    KTRY( db->push_heading( child, heading ) );
    KTRY( db->push_title( child, title ) );
    KTRY( db->push_child( parent, child ) );

    rv = outcome::success();

    return rv;
}

SCENARIO( "Network::create_child", "[benchmark][com][network]" )
{
    KM_RESULT_PROLOG();

    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const rn = REQUIRE_TRY( km.fetch_component< com::RootNode >() );

    GIVEN( "root" )
    {
        auto const root = rn->root_node();

        BENCHMARK( "create/erase_node" )
        {
            auto const c = KTRYE( nw->create_child( rn->root_node(), "0" ) );
            return ( nw->erase_node( c ) );
        };
        BENCHMARK_ADVANCED( "create/erase_internal_node" )( auto meter )
        {
            auto c = result::make_result< void >();
            auto const db = KTRYE( km.fetch_component< com::Database >() );
            auto const heading = "widget";
            meter.measure( [ & ]
            {
                auto cid = gen_uuid();
                KTRYE( db->push_node( cid ) );
                KTRYE( db->push_heading( cid, heading ) );
                KTRYE( db->push_title( cid, heading ) );
                KTRYE( db->push_child( rn->root_node(), cid ) );
                return db->erase_all( cid );
            } );
        };

        THEN( "create child with invalid heading fails" )
        {
            REQUIRE( !nw->create_child( root, "%" ) );
        }

        WHEN( "create child with heading" )
        {
            auto const cid = REQUIRE_TRY( nw->create_child( root, "1" ) );

            BENCHMARK( "KM_RESULT_PUSH_NODE( '/1' )" )
            {
                KM_RESULT_PROLOG();
                    KM_RESULT_PUSH_NODE( "cid", cid );
                return km_result_local_state;
            };

            THEN( "result is only child of root" )
            {
                auto const children = nw->fetch_children( root );

                REQUIRE( children.size() == 1 );
                REQUIRE( cid == *children.begin() );
            }
        }
    }
}

auto Network::erase_alias_desc( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto& astore = alias_store();

    KMAP_ENSURE( is_alias( id ), error_code::node::invalid_alias );
    KMAP_ENSURE( !is_top_alias( id ), error_code::node::invalid_alias );

    for( auto const children = anchor::node( id )
                             | view2::child
                             | view2::order
                             | act2::to_node_vec( km )
       ; auto const& child : children | views::reverse )
    {
        KTRY( erase_alias_desc( child ) );
    }

    KTRY( astore.erase_alias( AliasItem::alias_type{ id } ) );

    rv = outcome::success();

    return rv;
}

auto Network::erase_alias_leaf( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_alias( id ), error_code::node::invalid_alias );
    KMAP_ENSURE( !is_top_alias( id ), error_code::node::invalid_alias );

    KTRY( alias_store().erase_alias( AliasItem::alias_type{ id } ) );

    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        KTRY( estore.value()->fire_event( { "subject.network", "verb.erased", "object.node" },  { { "node_id", to_string( id ) } } ) );
    }

    rv = outcome::success();

    return rv;
}

auto Network::erase_alias_root( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const& astore = alias_store();
    auto const rsrc = resolve( id );
    auto const dst = KTRY( fetch_parent( id ) ); BC_ASSERT( dst == resolve( dst ) );

    KMAP_ENSURE( is_alias( id ), error_code::node::invalid_alias );
    KMAP_ENSURE( is_top_alias( id ), error_code::node::invalid_alias );

    { // Only erase alias dsts that map to this specific top alias, but not this specific alias (currently being erased).
        auto const rsrc_aliases = astore.fetch_aliases( AliasItem::rsrc_type{ dst } );

        for( auto const& alias : rsrc_aliases )
        {
            for( auto const children = anchor::node( alias )
                                     | view2::child
                                     | view2::order
                                     | act2::to_node_vec( km )
               ; auto const& child : children | views::reverse )
            {
                KTRY( erase_alias_desc( child ) );
            }
        }
    }

    for( auto const children = anchor::node( id )
                             | view2::child
                             | view2::order
                             | act2::to_node_vec( km )
       ; auto const& child : children | views::reverse )
    {
        KTRY( erase_alias_desc( child ) );
    }

    if( !attr::is_in_attr_tree( km, dst ) )
    {
        KTRY( attr::pop_order( km, dst, rsrc ) ); // TODO: Replace with event system.
    }

    KTRY( alias_store().erase_alias( AliasItem::alias_type{ id } ) ); // TODO: What happens if this fails? Need to undo db->erase_alias, for correctness.

    {
        auto const db = KTRY( fetch_component< com::Database >() );

        KTRY( db->erase_alias( rsrc, dst ) );
    }

    rv = outcome::success();

    return rv;
}

auto Network::erase_node_internal( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const db = KTRY( fetch_component< com::Database >() );

    BC_CONTRACT()
        BC_POST([ &
                , parent = fetch_parent( id ) ]
        {
            if( rv )
            {
                BC_ASSERT( !exists( id ) );

                if( parent ) // Not root
                {
                    BC_ASSERT( exists( parent.value() ) );
                    BC_ASSERT( !attr::is_in_order( km, parent.value(), id ) );
                }
            }
        })
    ;

    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );
    KMAP_ENSURE( !is_alias( id ) || is_top_alias( id ), error_code::network::invalid_node );
    KMAP_ENSURE( id != km.root_node_id(), error_code::network::invalid_node );

    if( is_top_alias( id ) )
    {
        KTRY( erase_alias_root( id ) );
    }
    else
    {
        // Delete children.
        auto const children = [ & ]
        {
            // TODO: Unify when attributes are allowed to be ordered.
            if( attr::is_in_attr_tree( km, id ) )
            {
                return anchor::node( id )
                     | view2::child
                     | act2::to_node_vec( km );
            }
            else
            {
                return anchor::node( id )
                     | view2::child
                     | view2::order
                     | act2::to_node_vec( km );
            }
        }();
        for( auto const& e : children | views::reverse ) // Not necessary to erase in reverse order, but it seems like a reasonable requirement (FILO)
        {
            if( is_top_alias( e ) )
            {
                KTRY( erase_alias_root( e ) );
            }
            else
            {
                KTRY( erase_node_internal( e ) );
            }
        }

        KTRY( erase_node_leaf( id ) );
    }

    rv = outcome::success();

    return rv;
}

auto Network::erase_node_leaf( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", id );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( id == resolve( id ) );
        })
        BC_POST([ &
                , parent = fetch_parent( id ) ]
        {
            if( rv )
            {
                BC_ASSERT( !exists( id ) );

                if( parent ) // Not root
                {
                    BC_ASSERT( exists( parent.value() ) );
                    BC_ASSERT( !attr::is_in_order( km, parent.value(), id ) );
                }
            }
        })
    ;

    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );
    KMAP_ENSURE( id != km.root_node_id(), error_code::network::invalid_node );
    KMAP_ENSURE( fetch_children( id ).empty(), error_code::network::invalid_node ); // Only applies to non-top-alias. AliasStore::erase_alias destroys descendants.

    auto const db = KTRY( fetch_component< com::Database >() );
    auto const aliases = astore_.fetch_aliases( AliasItem::rsrc_type{ id } );
    auto const desc_aliases = aliases
                            | rvs::filter( [ & ]( auto const& n ){ return !is_top_alias( n ); } )
                            | ranges::to< std::set< Uuid > >();
    auto const top_aliases = aliases
                           | rvs::filter( [ & ]( auto const& n ){ return is_top_alias( n ); } )
                           | ranges::to< std::set< Uuid > >();

    // TODO: Are not root and non-root alias children mutually exclusive?
    //       What is an example where they could co-exist?
    for( auto const& dalias : desc_aliases )
    {
        KTRY( erase_alias_leaf( dalias ) );
    }
    for( auto const& talias : top_aliases )
    {
        KTRY( erase_alias_root( talias ) );
    }

    if( !attr::is_in_attr_tree( km, id ) )
    {
        auto const parent = KTRY( fetch_parent( id ) );

        // TODO: Order should be done via event, I think? It's an add-on feature. Unless... event relies on order which complicates things.
        //       Update: order to become fundamental (database) concept, so ignore this TODO.
        KTRY( attr::pop_order( km, parent, id ) );

        if( auto const at = fetch_attr_node( id )
          ; at )
        {
            KTRY( erase_attr( at.value() ) );
        }
    }

    KTRY( db->erase_all( id ) );

    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        KTRY( estore.value()->fire_event( { "subject.network", "verb.erased", "object.node" },  { { "node_id", to_string( id ) } } ) );
    }

    rv = outcome::success();

    return rv;
}

// TODO: Unit test needed for alias as attribute child (e.g., a tag).
auto Network::erase_attr( Uuid const& id )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", id );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !attr::is_in_attr_tree( km, id ) );
            }
        })
    ;

    KMAP_ENSURE_MSG( attr::is_in_attr_tree( km, id ), error_code::network::invalid_node, to_string( id ) );
    
    for( auto const& child : fetch_children( id ) )
    {
        // TODO: Is this right? Child nodes of attributes are still child nodes, right?
        //       Negative... they are like ordinary nodes except without attributes themselves.
        //       Errghhh... this further complicates things if I use an alias as a $.tag., as the alias will then have attributes.
        //       But as long as such can be accounted for, it should be fine.
        KTRY( erase_node_internal( child ) );
    }

    auto const db = KTRY( fetch_component< com::Database >() );
    auto const parent = db->fetch_attr_owner( id );

    KTRY( db->erase_all( id ) );

    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        KTRY( estore.value()->fire_event( { "subject.network", "verb.erased", "object.node" },  { { "node_id", to_string( id ) } } ) );
    }

    rv = parent;

    return rv;
}


// TODO: impl. body belongs in path.cpp
auto Network::distance( Uuid const& ancestor
                      , Uuid const& descendant ) const
    -> uint32_t // TODO: Result< uint32_t >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "ancestor", ancestor );
        KM_RESULT_PUSH( "descendant", descendant );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( ancestor ) );
            BC_ASSERT( exists( descendant ) );
            BC_ASSERT( is_lineal( ancestor, descendant ) );
        })
    ;

    auto rv = uint32_t{};
    auto traveler = Optional< Uuid >{ descendant };

    while( traveler )
    {
        if( traveler != ancestor )
        {
            traveler = to_optional( fetch_parent( traveler.value() ) );

            ++rv;
        }
        else
        {
            traveler.reset();
        }
    }

    return rv;
}

// Returns a recommended node to select following the deletion of "id".
// TODO: Make test case for when node to be deleted is selected, when it's not, and when it's a descendant of the node to be deleted.
// TODO: Are we ensuring that attributes get cascade erased when parent node is erased? Unit tested?
auto Network::erase_node( Uuid const& id )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", id );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !exists( id ) ); 

                if( auto const p = fetch_parent( id )
                  ; p )
                {
                    BC_ASSERT( !attr::is_in_order( km, p.value(), id ) );
                }
            }
        })
    ;

    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );
    KMAP_ENSURE( !is_alias( id ) || is_top_alias( id ), error_code::network::invalid_node );
    KMAP_ENSURE( id != km.root_node_id(), error_code::node::is_root );

    auto const selected = selected_node();
    auto next_selected = selected;

    if( id == selected
     || is_ancestor( *this, id, selected ) )
    {
        auto const next_sel = KTRY( fetch_next_selected_as_if_erased( id ) );

        // TODO: This placement is concerning. Selection is done before erasure completes. Meaning selected fires.
        //       But to fire _before_ erasure would be premature. I doubt it would cause an error, but misleading, and the update
        //       to state wouldn't be reflected properly.
        KTRY( select_node( next_sel ) );

        next_selected = next_sel;
    }
    else
    {
        next_selected = selected; // If not deleting selected, just return selected.
    }

    KTRY( erase_node_internal( id ) );

    rv = next_selected;

    return rv;
}

SCENARIO( "Network::erase_node", "[network][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = kmap::Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "4 siblings who each alias sibling above: /1, /2, /3, /4: /1, /2.1, /3.2.1, /4.3.2.1" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const n3 = REQUIRE_TRY( nw->create_child( root, "3" ) );
        auto const n4 = REQUIRE_TRY( nw->create_child( root, "4" ) );
        auto const n21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );
        auto const n32 = REQUIRE_TRY( nw->create_alias( n2, n3 ) );
        auto const n321 = REQUIRE_TRY( anchor::node( n32 ) | view2::alias( view2::resolve( n1 ) ) | act2::fetch_node( km ) );
        auto const n43 = REQUIRE_TRY( nw->create_alias( n3, n4 ) );
        auto const n432 = REQUIRE_TRY( anchor::node( n43 ) | view2::alias( view2::resolve( n2 ) ) | act2::fetch_node( km ) );
        auto const n4321 = REQUIRE_TRY( anchor::node( n432 ) | view2::alias( view2::resolve( n1 ) ) | act2::fetch_node( km ) );

        WHEN( "erase_node( /1 )" )
        {
            REQUIRE_TRY( nw->erase_node( n1 ) );

            THEN( "[ ] /1" )       { REQUIRE( !nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[ ] /2.1" )     { REQUIRE( !nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[x] /3.2" )     { REQUIRE( nw->exists( n32 ) ); }
            THEN( "[ ] /3.2.1" )   { REQUIRE( !nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[x] /4.3" )     { REQUIRE( nw->exists( n43 ) ); }
            THEN( "[x] /4.3.2" )   { REQUIRE( nw->exists( n432 ) ); }
            THEN( "[ ] /4.3.2.1" ) { REQUIRE( !nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /2 )" )
        {
            REQUIRE_TRY( nw->erase_node( n2 ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[ ] /2" )       { REQUIRE( !nw->exists( n2 ) ); }
            THEN( "[ ] /2.1" )     { REQUIRE( !nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[ ] /3.2" )     { REQUIRE( !nw->exists( n32 ) ); }
            THEN( "[ ] /3.2.1" )   { REQUIRE( !nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[x] /4.3" )     { REQUIRE( nw->exists( n43 ) ); }
            THEN( "[ ] /4.3.2" )   { REQUIRE( !nw->exists( n432 ) ); }
            THEN( "[ ] /4.3.2.1" ) { REQUIRE( !nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /2.1 )" )
        {
            REQUIRE_TRY( nw->erase_node( n21 ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[ ] /2.1" )     { REQUIRE( !nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[x] /3.2" )     { REQUIRE( nw->exists( n32 ) ); }
            THEN( "[ ] /3.2.1" )   { REQUIRE( !nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[x] /4.3" )     { REQUIRE( nw->exists( n43 ) ); }
            THEN( "[x] /4.3.2" )   { REQUIRE( nw->exists( n432 ) ); }
            THEN( "[ ] /4.3.2.1" ) { REQUIRE( !nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /3 )" )
        {
            REQUIRE_TRY( nw->erase_node( n3 ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[x] /2.1" )     { REQUIRE( nw->exists( n21 ) ); }
            THEN( "[ ] /3" )       { REQUIRE( !nw->exists( n3 ) ); }
            THEN( "[ ] /3.2" )     { REQUIRE( !nw->exists( n32 ) ); }
            THEN( "[ ] /3.2.1" )   { REQUIRE( !nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[ ] /4.3" )     { REQUIRE( !nw->exists( n43 ) ); }
            THEN( "[ ] /4.3.2" )   { REQUIRE( !nw->exists( n432 ) ); }
            THEN( "[ ] /4.3.2.1" ) { REQUIRE( !nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /3.2 )" )
        {
            REQUIRE_TRY( nw->erase_node( n32 ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[x] /2.1" )     { REQUIRE( nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[ ] /3.2" )     { REQUIRE( !nw->exists( n32 ) ); }
            THEN( "[ ] /3.2.1" )   { REQUIRE( !nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[x] /4.3" )     { REQUIRE( nw->exists( n43 ) ); }
            THEN( "[ ] /4.3.2" )   { REQUIRE( !nw->exists( n432 ) ); }
            THEN( "[ ] /4.3.2.1" ) { REQUIRE( !nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /3.2.1 )" )
        {
            REQUIRE( test::fail( nw->erase_node( n321 ) ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[x] /2.1" )     { REQUIRE( nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[x] /3.2" )     { REQUIRE( nw->exists( n32 ) ); }
            THEN( "[x] /3.2.1" )   { REQUIRE( nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[x] /4.3" )     { REQUIRE( nw->exists( n43 ) ); }
            THEN( "[x] /4.3.2" )   { REQUIRE( nw->exists( n432 ) ); }
            THEN( "[x] /4.3.2.1" ) { REQUIRE( nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /4 )" )
        {
            REQUIRE_TRY( nw->erase_node( n4 ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[x] /2.1" )     { REQUIRE( nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[x] /3.2" )     { REQUIRE( nw->exists( n32 ) ); }
            THEN( "[x] /3.2.1" )   { REQUIRE( nw->exists( n321 ) ); }
            THEN( "[ ] /4" )       { REQUIRE( !nw->exists( n4 ) ); }
            THEN( "[ ] /4.3" )     { REQUIRE( !nw->exists( n43 ) ); }
            THEN( "[ ] /4.3.2" )   { REQUIRE( !nw->exists( n432 ) ); }
            THEN( "[ ] /4.3.2.1" ) { REQUIRE( !nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /4.3 )" )
        {
            REQUIRE_TRY( nw->erase_node( n43 ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[x] /2.1" )     { REQUIRE( nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[x] /3.2" )     { REQUIRE( nw->exists( n32 ) ); }
            THEN( "[x] /3.2.1" )   { REQUIRE( nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[ ] /4.3" )     { REQUIRE( !nw->exists( n43 ) ); }
            THEN( "[ ] /4.3.2" )   { REQUIRE( !nw->exists( n432 ) ); }
            THEN( "[ ] /4.3.2.1" ) { REQUIRE( !nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /4.3.2 )" )
        {
            REQUIRE( test::fail( nw->erase_node( n432 ) ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[x] /2.1" )     { REQUIRE( nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[x] /3.2" )     { REQUIRE( nw->exists( n32 ) ); }
            THEN( "[x] /3.2.1" )   { REQUIRE( nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[x] /4.3" )     { REQUIRE( nw->exists( n43 ) ); }
            THEN( "[x] /4.3.2" )   { REQUIRE( nw->exists( n432 ) ); }
            THEN( "[x] /4.3.2.1" ) { REQUIRE( nw->exists( n4321 ) ); }
        }
        WHEN( "erase_node( /4.3.2.1 )" )
        {
            REQUIRE( test::fail( nw->erase_node( n4321 ) ) );

            THEN( "[x] /1" )       { REQUIRE( nw->exists( n1 ) ); }
            THEN( "[x] /2" )       { REQUIRE( nw->exists( n2 ) ); }
            THEN( "[x] /2.1" )     { REQUIRE( nw->exists( n21 ) ); }
            THEN( "[x] /3" )       { REQUIRE( nw->exists( n3 ) ); }
            THEN( "[x] /3.2" )     { REQUIRE( nw->exists( n32 ) ); }
            THEN( "[x] /3.2.1" )   { REQUIRE( nw->exists( n321 ) ); }
            THEN( "[x] /4" )       { REQUIRE( nw->exists( n4 ) ); }
            THEN( "[x] /4.3" )     { REQUIRE( nw->exists( n43 ) ); }
            THEN( "[x] /4.3.2" )   { REQUIRE( nw->exists( n432 ) ); }
            THEN( "[x] /4.3.2.1" ) { REQUIRE( nw->exists( n4321 ) ); }
        }
    }
}

SCENARIO( "Network::erase_node erases attributes", "[network][attribute][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database", "network", "root_node" );

    auto& kmap = Singleton::instance();
    auto const db = REQUIRE_TRY( kmap.fetch_component< com::Database >() );
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const& attr_tbl = db->fetch< db::AttributeTable >();
    auto const& node_tbl = db->fetch< db::NodeTable >();
    auto const count = []( auto const& tbl ){ return std::distance( tbl.begin(), tbl.end() ); };
    auto const attr_original_count = count( attr_tbl );
    auto const node_original_count = count( node_tbl );

    GIVEN( "root node" )
    {
        GIVEN( "create.child 1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( kmap.root_node_id(), "1" ) );

            THEN( "new attribute nodes detected" )
            {
                REQUIRE( count( attr_tbl ) > attr_original_count );
                REQUIRE( count( node_tbl ) > node_original_count );
            }

            WHEN( "erase node" )
            {
                REQUIRE_RES( nw->erase_node( n1 ) );

                THEN( "attr nodes return to previous count" )
                {
                    REQUIRE( count( attr_tbl ) == attr_original_count );
                    REQUIRE( count( node_tbl ) == node_original_count );
                }
            }

            GIVEN( "1 attribute that is an alias" )
            {
                auto const n2 = REQUIRE_TRY( nw->create_child( kmap.root_node_id(), "2" ) );

                REQUIRE_TRY( view::make( n1 )
                           | view::attr
                           | view::child( "test_attr" )
                           | view::alias( view::Alias::Src{ n2 } )
                           | view::create_node( kmap ) );

                THEN( "erase node" )
                {
                    REQUIRE_TRY( nw->erase_node( n1 ) );
                }

                GIVEN( "1 is itself aliased" )
                {
                    auto const n3 = REQUIRE_TRY( nw->create_child( kmap.root_node_id(), "3" ) );

                    REQUIRE_TRY( nw->create_alias( n1, n3 ) );

                    THEN( "erase node" )
                    {
                        REQUIRE_TRY( nw->erase_node( n1 ) );
                    }
                }
            }
        }
    }
}

auto Network::exists( Uuid const& id ) const
    -> bool
{
    KM_RESULT_PROLOG();

    auto const db = KTRYE( fetch_component< com::Database >() );

    return db->node_exists( alias_store().resolve( id ) );
}

auto Network::exists( Heading const& heading ) const
    -> bool
{
    // TODO: Problem here. view::exists() fetches component com::Network, which if this is called before finishing being initialized, error!
    //       Probably best to remove this exists( heading ) from network altogether, and leave it to node_view.
    return view::abs_root
         | view::desc( heading )
         | view::exists( kmap_inst() );
}


auto Network::fetch_above( Uuid const& node ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( exists( node ), error_code::network::invalid_node );
    
    if( auto const children = anchor::node( node )
                            | view2::sibling_incl
                            | view2::order
                            | act2::to_node_vec( km )
      ; !children.empty() )
    {
        auto const it = find( children, node );

        if( it != begin( children ) )
        {
            rv = *std::prev( it );
        }
    }

    return rv;
}

SCENARIO( "Network::fetch_above", "[com][network]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const rn = REQUIRE_TRY( km.fetch_component< com::RootNode >() );

    GIVEN( "root" )
    {
        auto const root = rn->root_node();

        THEN( "root has no above" )
        {
            REQUIRE( test::fail( nw->fetch_above( root ) ) );
        }

        GIVEN( "root has child, c1" )
        {
            auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "c1 has no above" )
            {
                REQUIRE( test::fail( nw->fetch_above( c1 ) ) );
            }

            GIVEN( "root has another child, c2" )
            {
                auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                THEN( "c1 has no above" )
                {
                    REQUIRE( test::fail( nw->fetch_above( c1 ) ) );
                }
                THEN( "c2 has c1 above" )
                {
                    REQUIRE( c1 == REQUIRE_TRY( nw->fetch_above( c2 ) ) );
                }
            }
        }
    }
}

auto Network::fetch_below( Uuid const& node ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( exists( node ), error_code::network::invalid_node );

    auto const& km = kmap_inst();
    
    if( auto const children = anchor::node( node )
                            | view2::sibling_incl
                            | view2::order
                            | act2::to_node_vec( km )
      ; !children.empty() )
    {
        auto const it = find( children, node );

        if( it != end( children ) // TODO: I don't think it's even possible for it == end() here.
            && std::next( it ) != end( children ) )
        {
            rv = *std::next( it );
        }
    }

    return rv;
}

SCENARIO( "Network::fetch_below", "[com][network]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const rn = REQUIRE_TRY( km.fetch_component< com::RootNode >() );

    GIVEN( "root" )
    {
        auto const root = rn->root_node();

        THEN( "root has no below" )
        {
            REQUIRE( test::fail( nw->fetch_below( root ) ) );
        }

        GIVEN( "root has child, c1" )
        {
            auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "c1 has no below" )
            {
                REQUIRE( test::fail( nw->fetch_below( c1 ) ) );
            }

            GIVEN( "root has another child, c2" )
            {
                auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                THEN( "c2 has no below" )
                {
                    REQUIRE( test::fail( nw->fetch_below( c2 ) ) );
                }
                THEN( "c1 has c2 below" )
                {
                    REQUIRE( c2 == REQUIRE_TRY( nw->fetch_below( c1 ) ) );
                }
            }
        }
    }
}

// Equiv: anchor( id ) | left_lineal( alias_src ) ? // Well, we don't have an alias_src view, as of yet, so IDK...
// What is the purpose of this function... what does it do? It fetches all aliases whose source is a part of this node's lineage.
// Put another way: does any node alias a node in this lineage?
// Why it is important: so that these nodes can update their aliases to reflect the changes sources.
//                      Note that this needs to be recursive. If a source is updated, and that dst has an aliased ancestry, then that needs to be updated, and so on.
//                      ... while avoiding infinite recursion.
auto Network::fetch_aliased_ancestry( Uuid const& id ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( rv.size() <= ( view::make( id ) | view::ancestor | view::count( kmap_inst() ) ) );
        })
    ;

    auto child = id;
    auto parent = fetch_parent( child );
    
    while( parent )
    {
        if( auto const aliases_from = astore_.fetch_aliases( AliasItem::rsrc_type{ child } )
          ; aliases_from.size() > 0 )
        {
            rv.emplace_back( child );
        }

        child = parent.value();
        parent = fetch_parent( child );
    }

    return rv;
}

auto Network::fetch_attr_node( Uuid const& id ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", id );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const db = KTRY( fetch_component< com::Database >() );

    rv = KTRY( db->fetch_attr( alias_store().resolve( id ) ) ); // TODO: resolve( id ) is under debate. Should resolving aliases be done implicitly, or required explicitly? Here, it is implicit.

    return rv;
}

auto Network::fetch_body( Uuid const& node ) const
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = KMAP_MAKE_RESULT( std::string );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    if( exists( node ) )
    {
        auto const db = KTRY( fetch_component< com::Database >() );

        rv = KTRY( db->fetch_body( alias_store().resolve( node ) ) );
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::network::invalid_node, node );
    }

    return rv;
}

auto Network::fetch_children( Uuid const& parent ) const
    -> kmap::UuidSet // TODO: There's an argument that this should be Result< UuidSet >. If `parent` doesn't exist, that's more than just `parent` has zero children, it's an input error.
{ 
    KM_RESULT_PROLOG();

    auto rv = kmap::UuidSet{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( parent == KTRYE( fetch_parent( e ) ) );
            }
        })
    ;

    KMAP_ENSURE_EXCEPT( exists( parent ) ); // TODO: Replace with KM_ENSURE( exists( parent ) );
    KMAP_ENSURE_EXCEPT( fetch_heading( parent ) ); // TODO: Replace with KM_ENSURE( exists( parent ) );

    auto const db_children = [ & ]
    {
        if( is_alias( parent ) )
        {
            return kmap::UuidSet{};
        }
        else
        {
            auto const db = KTRYE( fetch_component< com::Database >() );

            return KTRYE( db->fetch_children( parent ) );
        }
    }();
    auto const alias_children = alias_store().fetch_alias_children( parent );

    auto const all = views::concat( db_children, alias_children )
                   | to< kmap::UuidSet >();

    return all;
    // TODO:
    // rv = all;
    // return rv;
}

auto Network::fetch_children_ordered( Uuid const& parent ) const
    -> Result< UuidVec >
{ 
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "parent", parent );

    auto rv = result::make_result< UuidVec >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                for( auto const& e : rv.value() )
                {
                    BC_ASSERT( parent == KTRYE( fetch_parent( e ) ) );
                }
            }
        })
    ;

    KMAP_ENSURE( exists( parent ), error_code::network::invalid_node );

    auto const& km = kmap_inst();
    auto const unord_children = fetch_children( parent );

    if( unord_children.empty() )
    {
        rv = UuidVec{};
    }
    else
    {
        auto const r_to_a_map = unord_children
                              | rvs::transform( [ & ]( auto const& n ){ return std::pair{ resolve( n ), n }; } )
                              | ranges::to< std::map >();
        auto const ordering_str = KTRY( anchor::node( parent )
                                      | view2::attr
                                      | view2::child( "order" )
                                      | act2::fetch_body( km ) );
        auto const split = ordering_str
                         | rvs::split( '\n' )
                         | ranges::to< std::vector< std::string > >();
        if( unord_children.size() != split.size() )
        {
            fmt::print( "unord_children:\n{}\n", unord_children | rvs::transform( [ & ]( auto const& e ){ return to_string( resolve( e ) ); } ) | rvs::join( '\n' ) | ranges::to< std::string >() );
            fmt::print( "ordering_str:\n{}\n", ordering_str );
        }
        KMAP_ENSURE( unord_children.size() == split.size(), error_code::common::uncategorized ); 

        auto const map_resolve = [ & ]( auto const& s )
        {  
            auto const order_node = KTRYE( uuid_from_string( s ) );
            // fmt::print( "r_to_a_map.at( '{}' )\n", s ); 
            if( !r_to_a_map.contains( order_node ) )
            {
                KMAP_THROW_EXCEPTION_MSG( fmt::format( "mismatch between children and order nodes, for node: {}\n", s ) );
            }
            return r_to_a_map.at( order_node );
        };

        rv = split
           | rvs::transform( map_resolve )
           | ranges::to< std::vector >();
    }

    return rv;
}

SCENARIO( "Network::fetch_children_ordered", "[network][iface][order]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = km.root_node_id();

    GIVEN( "/" )
    {
        THEN( "foc( / ) => {}" )
        {
            auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( root ) );

            REQUIRE( ov == UuidVec{} );
        }

        GIVEN( "/1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "foc( / ) => { 1 }" )
            {
                auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( root ) );

                REQUIRE( ov == UuidVec{ n1 } );
            }

            GIVEN( "/2" )
            {
                auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                THEN( "fco( / ) => { 1, 2 }" )
                {
                    auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( root ) );

                    REQUIRE( ov == UuidVec{ n1, n2 } );
                }

                GIVEN( "/3" )
                {
                    auto const n3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

                    THEN( "fco( / ) => { 1, 2, 3 }" )
                    {
                        auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( root ) );

                        REQUIRE( ov == UuidVec{ n1, n2, n3 } );
                    }

                    GIVEN( "alias /2.1[/1]" )
                    {
                        auto const a21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );

                        THEN( "fco( 2 ) => { 2.1[/1] }" )
                        {
                            auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( n2 ) );

                            REQUIRE( ov == UuidVec{ a21 } );
                        }
                        THEN( "fco( 2.1[/1] ) => {}" )
                        {
                            auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a21 ) );

                            REQUIRE( ov == UuidVec{} );
                        }

                        GIVEN( "alias /3.2[/2]" )
                        {
                            auto const a32 = REQUIRE_TRY( nw->create_alias( n2, n3 ) );
                            auto const a321 = REQUIRE_TRY( anchor::node( a32 ) | view2::alias | act2::fetch_node( km ) );

                            THEN( "fco( 3 ) => { 3.2[/2] }" )
                            {
                                auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( n3 ) );

                                REQUIRE( ov == UuidVec{ a32 } );
                            }
                            THEN( "fco( 3.2[/2] ) => { 3.2.1[/1] }" )
                            {
                                auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a32 ) );

                                REQUIRE( ov == UuidVec{ a321 } );
                            }
                            THEN( "fco( 3.2.1[/1] ) => {}" )
                            {
                                auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a321 ) );

                                REQUIRE( ov == UuidVec{} );
                            }

                            GIVEN( "/1.x" )
                            {
                                auto const n1x = REQUIRE_TRY( nw->create_child( n1, "x" ) );
                                auto const a21x = REQUIRE_TRY( anchor::node( a21 ) | view2::alias( view2::resolve( n1x ) ) | act2::fetch_node( km ) );
                                auto const a321x = REQUIRE_TRY( anchor::node( a321 ) | view2::alias( view2::resolve( n1x ) ) | act2::fetch_node( km ) );

                                THEN( "fco( 1 ) => { 1.x }" )
                                {
                                    auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( n1 ) );

                                    REQUIRE( ov == UuidVec{ n1x } );
                                }
                                THEN( "fco( 1.x ) => {}" )
                                {
                                    auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( n1x ) );

                                    REQUIRE( ov == UuidVec{} );
                                }
                                THEN( "fco( 2.1 ) => { 2.1.x[/1.x] }" )
                                {
                                    auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a21 ) );

                                    REQUIRE( ov == UuidVec{ a21x } );
                                }
                                THEN( "fco( 2.1.x[/1.x] ) => {}" )
                                {
                                    auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a21x ) );

                                    REQUIRE( ov == UuidVec{} );
                                }
                                THEN( "fco( 3.2.1 ) => { 3.2.1.x[/1.x] }" )
                                {
                                    auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a321 ) );

                                    REQUIRE( ov == UuidVec{ a321x } );
                                }
                                THEN( "fco( 3.2.1.x[/1.x] ) => {}" )
                                {
                                    auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a321x ) );

                                    REQUIRE( ov == UuidVec{} );
                                }

                                GIVEN( "/1.y" )
                                {
                                    auto const n1y = REQUIRE_TRY( nw->create_child( n1, "y" ) );
                                    auto const a21y = REQUIRE_TRY( anchor::node( a21 ) | view2::alias( view2::resolve( n1y ) ) | act2::fetch_node( km ) );
                                    auto const a321y = REQUIRE_TRY( anchor::node( a321 ) | view2::alias( view2::resolve( n1y ) ) | act2::fetch_node( km ) );

                                    THEN( "fco( 1 ) => { 1.x, 1.y }" )
                                    {
                                        auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( n1 ) );

                                        REQUIRE( ov == UuidVec{ n1x, n1y } );
                                    }
                                    THEN( "fco( 2.1 ) => { 2.1.x[/1.x], 2.1.y[/1.y] }" )
                                    {
                                        auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a21 ) );

                                        REQUIRE( ov == UuidVec{ a21x, a21y } );
                                    }
                                    THEN( "fco( 3.2.1 ) => { 3.2.1.x[/1.x], 3.2.1.y[/1.y] }" )
                                    {
                                        auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a321 ) );

                                        REQUIRE( ov == UuidVec{ a321x, a321y } );
                                    }

                                    GIVEN( "/1.z" )
                                    {
                                        KMAP_LOG_LINE();
                                        auto const n1z = REQUIRE_TRY( nw->create_child( n1, "z" ) );
                                        KMAP_LOG_LINE();
                                        auto const a21z = REQUIRE_TRY( anchor::node( a21 ) | view2::alias( view2::resolve( n1z ) ) | act2::fetch_node( km ) );
                                        auto const a321z = REQUIRE_TRY( anchor::node( a321 ) | view2::alias( view2::resolve( n1z ) ) | act2::fetch_node( km ) );

                                        THEN( "fco( 1 ) => { 1.x, 1.y, 1.z }" )
                                        {
                                            auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( n1 ) );

                                            REQUIRE( ov == UuidVec{ n1x, n1y, n1z } );
                                        }
                                        THEN( "fco( 2.1 ) => { 2.1.x[/1.x], 2.1.y[/1.y], 2.1.z[/1.z] }" )
                                        {
                                            auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a21 ) );

                                            REQUIRE( ov == UuidVec{ a21x, a21y, a21z } );
                                        }
                                        THEN( "fco( 3.2.1 ) => { 3.2.1.x[/1.x], 3.2.1.y[/1.y], 3.2.1.y[/1.z] }" )
                                        {
                                            auto const ov = REQUIRE_TRY( nw->fetch_children_ordered( a321 ) );

                                            REQUIRE( ov == UuidVec{ a321x, a321y, a321z } );
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

auto Network::fetch_heading( Uuid const& node ) const
    -> Result< Heading >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Heading );

    if( exists( node ) )
    {
        auto const db = KTRY( fetch_component< com::Database >() );

        rv = KMAP_TRY( db->fetch_heading( alias_store().resolve( node ) ) );
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::network::invalid_node, node );
    }

    return rv;
}

auto Network::fetch_genesis_time( Uuid const& id ) const
    -> Optional< uint64_t >
{
    KM_RESULT_PROLOG();

    auto const db = KTRYE( fetch_component< com::Database >() );

    return db->fetch_genesis_time( id );
}

auto Network::fetch_nodes( Heading const& heading ) const
    -> UuidSet
{
    KM_RESULT_PROLOG();

    auto const db = KTRYE( fetch_component< com::Database >() );

    return db->fetch_nodes( heading );
}

auto Network::fetch_ordering_position( Uuid const& node ) const
    -> Result< uint32_t >
{
    using Map = std::vector< std::pair< Uuid, uint64_t > >;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = KMAP_MAKE_RESULT( uint32_t );

    auto const& km = kmap_inst();
    auto const parent = KTRY( fetch_parent( node ) );
    auto const children = fetch_children( parent );
    auto const rnode = resolve( node );

    BC_ASSERT( !children.empty() );

    auto map = Map{};
    auto const porder = KTRY( view::make( parent )
                            | view::resolve
                            | view::attr
                            | view::child( "order" )
                            | view::fetch_node( km ) );
    auto const body = KTRY( fetch_body( porder ) );
    auto id_ordinal_map = std::map< Uuid, uint64_t >{};
    auto const ids = body
                   | views::split( '\n' )
                   | to< std::vector< std::string > >();

    for( auto const [ i, s ] : ids | views::enumerate )
    {
        auto const idi = KTRYE( to_uuid( s ) );

        BC_ASSERT( !alias_store().is_alias( idi ) );

        id_ordinal_map.emplace( idi, i );
    }
    
    // TODO: This is more of a sanity check, right? Not exactly something that would go in Release.
    for( auto const& e : children )
    {
        if( !id_ordinal_map.contains( alias_store().resolve( e ) ) )
        {
            for( auto const& ord : id_ordinal_map )
            {
                fmt::print( "ord: {}, alias?:{}\n", ord.first, to_string( alias_store().resolve( ord.first ) ) );
            }
            KMAP_THROW_EXCEPTION_MSG( fmt::format( "failed to find ordering ({}) for child: {}", to_string( parent ), to_string( e ) ) );
        }
    }

    rv = id_ordinal_map.at( rnode );

    return rv;
}

SCENARIO( "Network::fetch_ordering_position", "[network][iface][order]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const root = kmap.root_node_id();

    GIVEN( "alias child" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const c3 = REQUIRE_TRY( nw->create_child( root, "3" ) );
        auto const a21 = REQUIRE_TRY( nw->create_alias( c2, c1 ) );
        auto const a31 = REQUIRE_TRY( nw->create_alias( c3, c1 ) );

        WHEN( "fetch_ordering_position of alias parent" )
        {
            THEN( "aliases are ordered properly" )
            {
                auto const a21_pos = REQUIRE_TRY( nw->fetch_ordering_position( a21 ) );
                auto const a31_pos = REQUIRE_TRY( nw->fetch_ordering_position( a31 ) );
                REQUIRE( a21_pos == 0 );
                REQUIRE( a31_pos == 1 );
            }
        }
    }
}

auto Network::fetch_next_selected_as_if_erased( Uuid const& node ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    if( auto const above = fetch_above( node )
      ; above )
    {
        rv = above.value();
    }
    else if( auto const below = fetch_below( node )
           ; below )
    {
        rv = below.value();
    }
    else if( auto const parent = fetch_parent( node )
           ; parent )
    {
        rv = parent.value();
    }
    
    return rv;
}

auto Network::resolve( Uuid const& node ) const
    -> Uuid
{
    return astore_.resolve( node );
}

SCENARIO( "Network::resolve", "[alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = kmap::Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "4 siblings who each alias sibling above: /1, /2, /3, /4: /1, /2.1, /3.2.1, /4.3.2.1" )
    {
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const n3 = REQUIRE_TRY( nw->create_child( root, "3" ) );
        auto const n4 = REQUIRE_TRY( nw->create_child( root, "4" ) );
        auto const n21 = REQUIRE_TRY( nw->create_alias( n1, n2 ) );
        auto const n32 = REQUIRE_TRY( nw->create_alias( n2, n3 ) );
        auto const n321 = REQUIRE_TRY( anchor::node( n32 ) | view2::alias( view2::resolve( n1 ) ) | act2::fetch_node( km ) );
        auto const n43 = REQUIRE_TRY( nw->create_alias( n3, n4 ) );
        auto const n432 = REQUIRE_TRY( anchor::node( n43 ) | view2::alias( view2::resolve( n2 ) ) | act2::fetch_node( km ) );
        auto const n4321 = REQUIRE_TRY( anchor::node( n432 ) | view2::alias( view2::resolve( n1 ) ) | act2::fetch_node( km ) );

        THEN( "resolve( /1 ) == /1" ) { REQUIRE( nw->resolve( n1 ) == n1 ); }
        THEN( "resolve( /2 ) == /2" ) { REQUIRE( nw->resolve( n2 ) == n2 ); }
        THEN( "resolve( /2.1 ) == /1" ) { REQUIRE( nw->resolve( n21 ) == n1 ); }
        THEN( "resolve( /3 ) == /3" ) { REQUIRE( nw->resolve( n3 ) == n3 ); }
        THEN( "resolve( /3.2 ) == /2" ) { REQUIRE( nw->resolve( n32 ) == n2 ); }
        THEN( "resolve( /3.2.1 ) == /1" ) { REQUIRE( nw->resolve( n321 ) == n1 ); }
        THEN( "resolve( /4 ) == /4" ) { REQUIRE( nw->resolve( n4 ) == n4 ); }
        THEN( "resolve( /4.3 ) == /3" ) { REQUIRE( nw->resolve( n43 ) == n3 ); }
        THEN( "resolve( /4.3.2 ) == /2" ) { REQUIRE( nw->resolve( n432 ) == n2 ); }
        THEN( "resolve( /4.3.2.1 ) == /1" ) { REQUIRE( nw->resolve( n4321 ) == n1 ); }
    }
}

auto Network::fetch_parent( Uuid const& child ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH( "child", child ); // Warning: This is prone to recursive invocation if fetch_parent itself fails, as AbsPathFlat uses fetch_parent to describe the arg node.

    auto rv = KMAP_MAKE_RESULT_EC( Uuid, error_code::network::invalid_parent );
    auto const db = KTRY( fetch_component< com::Database >() );

    if( is_alias( child ) )
    {
        rv = KTRY( alias_store().fetch_parent( child ) );
    }
    else
    {
        rv = KTRY( db->fetch_parent( child ) );
    }

    return rv;
}

SCENARIO( "Network::fetch_parent", "[com][network]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "root" )
    {
        THEN( "no parent" )
        {
            REQUIRE( test::fail( nw->fetch_parent( root ) ) );
        }

        GIVEN( "child 1" )
        {
            auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "parent is root" )
            {
                REQUIRE( root == REQUIRE_TRY( nw->fetch_parent( c1 ) ) );
            }

            GIVEN( "sibling 2" )
            {
                auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                THEN( "parent is root " )
                {
                    auto const c1p = REQUIRE_TRY( nw->fetch_parent( c2 ) );
                    REQUIRE( root == c1p );
                }
            }
        }

        GIVEN( "/[1,2,3] aliases: 2.1, 3.2.1" )
        {
            auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
            auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
            auto const c3 = REQUIRE_TRY( nw->create_child( root, "3" ) );
            auto const a21 = REQUIRE_TRY( nw->create_alias( c1, c2 ) );
            auto const a32 = REQUIRE_TRY( nw->create_alias( c2, c3 ) );
            auto const a321 = REQUIRE_TRY( view::make( a32 ) | view::child | view::fetch_node( km ) );

            THEN( "2.1 parent is 2" )
            {
                auto const a21p = REQUIRE_TRY( nw->fetch_parent( a21 ) );
                REQUIRE( c2 == a21p );
            }
            THEN( "3.2.1 parent is 2" )
            {
                auto const a321p = REQUIRE_TRY( nw->fetch_parent( a321 ) );
                REQUIRE( a32 == a321p );
            }
            THEN( "3.2 parent is 3" )
            {
                auto const a32p = REQUIRE_TRY( nw->fetch_parent( a32 ) );
                REQUIRE( c3 == a32p );
            }
        }
    }
}

auto Network::fetch_title( Uuid const& id ) const
    -> Result< Title >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", id );

    auto rv = KMAP_MAKE_RESULT( Title );

    if( exists( id ) )
    {
        auto const db = KTRY( fetch_component< com::Database >() );

        rv = db->fetch_title( alias_store().resolve( id ) ).value();
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::network::invalid_node, id );
    }

    return rv;
}

SCENARIO( "Network::fetch_title", "[com][network]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const rn = REQUIRE_TRY( km.fetch_component< com::RootNode >() );

    GIVEN( "root" )
    {
        auto const root = rn->root_node();

        THEN( "root title matches database entry" )
        {
            auto const db = REQUIRE_TRY( km.fetch_component< com::Database >() );
            auto const nw_rt = REQUIRE_TRY( nw->fetch_title( root ) );
            auto const db_rt = REQUIRE_TRY( db->fetch_title( root ) );

            REQUIRE( nw_rt == db_rt );
        }

        WHEN( "create child with heading" )
        {
            auto const h1 = "1";
            auto const c1 = REQUIRE_TRY( nw->create_child( root, h1 ) );

            THEN( "title is as expected" )
            {
                auto const fh1 = REQUIRE_TRY( nw->fetch_title( c1 ) );

                REQUIRE( fh1 == h1 );
            }
        }
    }
}

auto Network::move_body( Uuid const& src
                       , Uuid const& dst )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src", src );
        KM_RESULT_PUSH( "dst", dst );

    return kmap::move_body( *this, src, dst );
}

// TODO: Should this ensure that if the moved node is the selected node, it updates the selected as well? I believe this is only relevant for the alias case.
//       In essence, this can invalidate IDs for aliases.
// TODO: It's important to have a fail-safe for this routine. If remove succeeds, but, for some reason, add fails, the remove should revert (or vice-versa),
//       otherwise we're at risk of having a dangling node.
auto Network::move_node( Uuid const& from
                       , Uuid const& to )
    -> Result< Uuid > // TODO: Why is this returning "to"? Probably better to return void?
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src", from );
        KM_RESULT_PUSH( "dst", to );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ &
                , is_from_an_alias = alias_store().is_alias( from ) ]
        {
            if( rv )
            {
                if( !is_from_an_alias )
                {
                    BC_ASSERT( is_child( to, from ) );
                    BC_ASSERT( attr::is_in_order( km, to, from ) );
                }
            }
        })
    ;

    KMAP_ENSURE( exists( from ), error_code::network::invalid_node );
    KMAP_ENSURE( exists( to ), error_code::network::invalid_node );
    KMAP_ENSURE( !is_child( to, fetch_heading( from ).value() ), error_code::network::invalid_heading ); // TODO: Replace invalid_heading with duplicate_heading. See create_node::duplicate_child_heading.
    KMAP_ENSURE( !alias_store().is_alias( to ), error_code::network::invalid_node );
    KMAP_ENSURE( from != km.root_node_id(), error_code::network::invalid_node );
    KMAP_ENSURE( !is_ancestor( *this, from, to ), error_code::network::invalid_node );

    auto const from_parent = fetch_parent( from ).value();
    auto const from_heading = fetch_heading( from ).value();

    if( is_child( to, from ) )
    {
        rv = from; // Already moved "self-move". Nothing to do.
    }
    else if( is_alias( from ) )
    {
        auto const db = KTRY( fetch_component< com::Database >() );
        auto const rfrom = resolve( from );

        KTRY( erase_alias_root( from ) );

        auto const alias = KTRY( create_alias( rfrom, to ) );

        rv = alias;
    }
    else
    {
        auto const db = KTRY( fetch_component< com::Database >() );

        KTRY( db->erase_child( from_parent, from ) );
        KTRY( db->push_child( to, from ) );

        KTRY( attr::pop_order( km, from_parent, from ) );
        KTRY( attr::push_order( km, to, from ) );

        if( auto const estore = fetch_component< com::EventStore >()
          ; estore )
        {
            KTRY( estore.value()->fire_event( { "subject.network", "verb.moved", "object.node" }
                                            , { { "child_node", to_string( from ) }
                                              , { "old_parent_node", to_string( from_parent ) }
                                              , { "new_parent_node", to_string( to ) } } ) );
        }

        rv = to;
    }

    return rv;
}

SCENARIO( "Network::move_node", "[network][iface][move_node][order]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = km.root_node_id();

    GIVEN( "root" )
    {
        GIVEN( "child 1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            WHEN( "move to root; to same location: fail" )
            {
                REQUIRE( !nw->move_node( n1, root ) );
            }

            GIVEN( "child 2" )
            {
                auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                WHEN( "move 2 to 1" )
                {
                    REQUIRE_TRY( nw->move_node( n2, n1 ) );

                    THEN( "2 is proper child of 1" )
                    {
                        REQUIRE( nw->is_child( n1, n2 ) );
                        REQUIRE( attr::is_in_order( km, n1, n2 ) );
                    }
                }
            }
        }
    }
}

// Note: 'children' should be unresolved. TODO: Actually, I suspect this is only a requirement for the precondition, which could always resolve all IDs. To confirm. I believe, when it comes to ordering, the resolved node is the only one used.
auto Network::reorder_children( Uuid const& parent
                              , std::vector< Uuid > const& children )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "parent", parent );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                auto const ordern = KTRYE( view::make( parent )
                                         | view::attr
                                         | view::child( "order" )
                                         | view::fetch_node( km ) );
                auto const body = KTRYE( fetch_body( ordern ) );
                auto const split = body
                               | rvs::split( '\n' )
                               | ranges::to< std::vector< std::string > >();
                auto const ids = split
                               | rvs::transform( [ & ](auto const& e ){ return KTRYE( uuid_from_string( e ) ); } )
                               | ranges::to< UuidSet >();
                BC_ASSERT( ids == ( children | rvs::transform( [ & ]( auto const& e ){ return resolve( e ); } ) | ranges::to< UuidSet >() ) );
            }
        })
    ;

    KMAP_ENSURE( fetch_children( parent ) == ( children | to< std::set >() ), error_code::network::invalid_ordering );

    auto const osv = children
                   | views::transform( [ & ]( auto const& e ){ return to_string( alias_store().resolve( e ) ); } )
                   | to< std::vector >();
    auto const oss = osv
                   | views::join( '\n' )
                   | to< std::string >();
    auto const ordern = KTRY( view::make( parent )
                            | view::attr
                            | view::child( "order" )
                            | view::fetch_node( km ) );

    // fmt::print( "updating children for parent({}): {}\n", to_string( parent ), oss | views::replace( '\n', '|' ) | to< std::string >() );
    
    KTRY( update_body( ordern, oss ) );

    rv = outcome::success();

    return rv;
}

// Note: Because Network requires RootNode, it is safe to provide this utility with naked result.
auto Network::root_node() const
    -> Uuid
{
    KM_RESULT_PROLOG();

    auto const rn = KTRYE( fetch_component< com::RootNode >() );

    return rn->root_node();
}

auto Network::swap_nodes( Uuid const& t1
                        , Uuid const& t2 )
    -> Result< UuidPair >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "n1", t1 );
        KM_RESULT_PUSH( "n2", t2 );

    auto rv = KMAP_MAKE_RESULT( UuidPair );

    BC_CONTRACT()
        BC_POST([ &
                , t1_pos = BC_OLDOF( fetch_ordering_position( t1 ) )
                , t2_pos = BC_OLDOF( fetch_ordering_position( t2 ) )
                , t1_parent = BC_OLDOF( fetch_parent( t1 ) )
                , t2_parent = BC_OLDOF( fetch_parent( t2 ) ) ]
        {
            if( rv )
            {
                BC_ASSERT( is_child( t1_parent->value(), t2 ) );
                BC_ASSERT( is_child( t2_parent->value(), t1 ) );
                BC_ASSERT( t1_pos->value() == fetch_ordering_position( t2 ).value() );
                BC_ASSERT( t2_pos->value() == fetch_ordering_position( t1 ).value() );
            }
        })
    ;

    // TODO: Can use Boost.Outcome Custom Payload to denote which node is not found.
    KMAP_ENSURE( exists( t1 ), error_code::node::not_found );
    KMAP_ENSURE( exists( t2 ), error_code::node::not_found );
    KMAP_ENSURE( !is_root( t1 ), error_code::node::is_root );
    KMAP_ENSURE( !is_root( t2 ), error_code::node::is_root );
    KMAP_ENSURE( !is_lineal( t1, t2 ), error_code::node::is_lineal );
    KMAP_ENSURE( !is_lineal( t2, t1 ), error_code::node::is_lineal );

    auto const t1_pos = KTRY( fetch_ordering_position( t1 ) );
    auto const t2_pos = KTRY( fetch_ordering_position( t2 ) );
    auto const t1_parent = KTRY( fetch_parent( t1 ) );
    auto const t2_parent = KTRY( fetch_parent( t2 ) );

    if( t1_parent != t2_parent ) // No movement is required if nodes are siblings; only repositioning.
    {
        KTRY( move_node( t1, t2_parent ) );
        KTRY( move_node( t2, t1_parent ) );
    }

    KTRY( set_ordering_position( t1, t2_pos ) ); 
    KTRY( set_ordering_position( t2, t1_pos ) ); 

    rv = std::pair{ t2, t1 };

    return rv;
}

SCENARIO( "swap two sibling aliases", "[network][iface][swap_nodes][order]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const root = kmap.root_node_id();

    GIVEN( "two sibling aliases" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const c3 = REQUIRE_TRY( nw->create_child( root, "3" ) );
        auto const a31 = REQUIRE_TRY( nw->create_alias( c1, c3 ) );
        auto const a32 = REQUIRE_TRY( nw->create_alias( c2, c3 ) );

        THEN( "aliases are ordered properly" )
        {
            auto const a31_op = REQUIRE_TRY( nw->fetch_ordering_position( a31 ) );
            auto const a32_op = REQUIRE_TRY( nw->fetch_ordering_position( a32 ) );
            REQUIRE( a31_op == 0 );
            REQUIRE( a32_op == 1 );
        }

        WHEN( "swap aliases" )
        {
            REQUIRE_RES( nw->swap_nodes( a31, a32 ) );

            THEN( "aliases swapped" )
            {
                auto const a31_op = REQUIRE_TRY( nw->fetch_ordering_position( a31 ) );
                auto const a32_op = REQUIRE_TRY( nw->fetch_ordering_position( a32 ) );
                REQUIRE( a31_op == 1 );
                REQUIRE( a32_op == 0 );
            }

            WHEN( "swap aliases again" )
            {
                REQUIRE_RES( nw->swap_nodes( a31, a32 ) );

                THEN( "aliases swapped" )
                {
                    auto const a31_op = REQUIRE_TRY( nw->fetch_ordering_position( a31 ) );
                    auto const a32_op = REQUIRE_TRY( nw->fetch_ordering_position( a32 ) );
                    REQUIRE( a31_op == 0 );
                    REQUIRE( a32_op == 1 );
                }
            }
        }
    }
}

// Returns previously selected node.
auto Network::select_node( Uuid const& id )
    -> Result< Uuid > 
{
    KMAP_PROFILE_SCOPE();

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", id );

    auto rv = result::make_result< Uuid >();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( selected_node_ != Uuid{ 0 } );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( selected_node() == id );
                // assert viewport is centered...
                // assert nw is focused...
                // assert jump stack is increased...(possibly, depends)
            }
        })
    ;

    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );

    auto const prev_selected = selected_node();
    selected_node_ = id;

    // TODO: Calling upon com::EventStore is problematic, as EventStore relies on this, com::Network, in initialization order.
    //       For now, checking for corner cases when this situation occurs, and not firing when event_store is not available.
    //       Cyclic dependency. EventStore relies on Network, and Network, here, is relying on EventStore.
    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        auto const& estorev = estore.value();

        KTRY( estorev->install_subject( "kmap" ) );
        KTRY( estorev->install_verb( "selected") );
        KTRY( estorev->install_object( "node") );
        KTRY( estorev->fire_event( { "subject.network", "verb.selected", "object.node" }
                                 , { { "from_node", to_string( prev_selected ) }
                                   , { "to_node", to_string( id ) } } ) );
    }
    
    // TODO:
    // Q: I notice that the event system (above estore->fire_event) introduces an unpredictability into the flow.
    //    Fire this thing mid flow, then a listener can do _whatever_ it wants (no restrictions), and hopefully the rest of the function (or greater flow)
    //    can complete without being broken/compromised.
    //    There must be a safer way, such as locking previous and currently selected nodes before firing. Something like that.
    rv = prev_selected;

    return rv;
}

auto Network::select_node_initial( Uuid const& node )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = KMAP_MAKE_RESULT( void );

fmt::print( "selected_node_: {}\n", to_string( selected_node_ ) );
    KMAP_ENSURE( node != Uuid{ 0 }, error_code::common::uncategorized );
    KMAP_ENSURE( selected_node_ == Uuid{ 0 }, error_code::common::uncategorized );

    selected_node_ = node;
    // TODO: fire selected node event?

    rv = outcome::success();

    return rv;
}

auto Network::selected_node() const
    -> Uuid
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( selected_node_ != Uuid{ 0 } );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( selected_node_ != Uuid{ 0 } );
        })
    ;

    return selected_node_;
}

auto Network::set_ordering_position( Uuid const& id
                                   , uint32_t pos )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", id );
        KM_RESULT_PUSH( "pos", std::to_string( pos ) );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_ordering_position( id ) );
                BC_ASSERT( fetch_ordering_position( id ).value() == pos );
            }
        })
    ;

    if( auto siblings = ( anchor::node( id ) 
                        | view2::parent
                        | view2::child
                        | view2::order
                        | act2::to_node_vec( km ) )
      ; siblings.size() > pos )
    {
        auto const parent = KTRY( fetch_parent( id ) );
        auto const id_it = find( siblings, id );

        BC_ASSERT( id_it != end( siblings) );

        siblings.erase( id_it );
        siblings.insert( begin( siblings ) + pos, id );

        KTRY( reorder_children( parent, siblings ) );

        rv = outcome::success();
    }

    return rv;
}

SCENARIO( "Network::set_ordering_position", "[com][network]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/1" )
    {
        // TODO: I think I've solved the outstanding issue - just hoping I haven't introduced another.
        //       Once some basic testing is complete (especially for boundary cases) should be able to call it good.
        auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

        GIVEN( "/1.2" )
        {
            auto const n2 = REQUIRE_TRY( nw->create_child( n1, "2" ) );

            THEN( "sop( 1.2, 0 ) => 0" )
            {
                REQUIRE_TRY( nw->set_ordering_position( n2, 0 ) );
                REQUIRE( REQUIRE_TRY( nw->fetch_ordering_position( n2 ) ) == 0 );
            }
            THEN( "sop( 1.2, 1 ) => fail" )
            {
                REQUIRE( test::fail( nw->set_ordering_position( n2, 1 ) ) );
                REQUIRE( REQUIRE_TRY( nw->fetch_ordering_position( n2 ) ) == 0 );
            }
            THEN( "sop( 1.2, 2 ) => fail" )
            {
                REQUIRE( test::fail( nw->set_ordering_position( n2, 2 ) ) );
                REQUIRE( REQUIRE_TRY( nw->fetch_ordering_position( n2 ) ) == 0 );
            }

            GIVEN( "/1.3" )
            {
                REQUIRE_TRY( nw->create_child( n1, "3" ) );

                THEN( "sop( 1.2, 0 ) => 0" )
                {
                    REQUIRE_TRY( nw->set_ordering_position( n2, 0 ) );
                    REQUIRE( REQUIRE_TRY( nw->fetch_ordering_position( n2 ) ) == 0 );
                }
                THEN( "sop( 1.2, 1 ) => 1" )
                {
                    REQUIRE_TRY( nw->set_ordering_position( n2, 1 ) );
                    REQUIRE( REQUIRE_TRY( nw->fetch_ordering_position( n2 ) ) == 1 );
                }
                THEN( "sop( 1.2, 2 ) => fail" )
                {
                    REQUIRE( test::fail( nw->set_ordering_position( n2, 2 ) ) );
                    REQUIRE( REQUIRE_TRY( nw->fetch_ordering_position( n2 ) ) == 0 );
                }
            }
        }
    }
}

auto Network::update_alias( Uuid const& from
                          , Uuid const& to )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src", from );
        KM_RESULT_PUSH( "dst", to );

    auto rv = result::make_result< Uuid >();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    KMAP_ENSURE( exists( from ), error_code::network::invalid_node );
    KMAP_ENSURE( exists( to ), error_code::network::invalid_node );

    // TODO: Deleting and remaking all does not seem optimal way to update.
    // TODO: This may actually be a problematic way of updating the alias. Why not just check if it exists, and if not, create it?
    //       Reason: The cache SM has a property that an entry shouldn't be recreated in the same flush cycle. I believe I did this as a sanity check,
    //       i.e., if node X is created, erased, and deleted within the same cache cycle, surely something has gone wrong, right?
    //       Well, IDK. Some Uuids are static/hardcoded, so if they get recreated... Likewise, this case, where an alias is erased => recreated on update.
    //       Whether that sanity check should be there or not, I'd rather keep it, if possible, for sanity.
    //       Update: I don't follow the line about alias recreation effecting the cache. Aliases IDs are not maintained by the cache.
    // TODO: Care needs to be taken not to trigger cyclic events while handling events.
    //       E.g., if update_alias gets called in response to a node manip event, then `erase_node` can't fire another node manip event.
    KTRY( erase_node( make_alias_id( from, to ) ) );

    rv = KTRY( create_alias( from, to ) );

    return rv;
}

// Note: I would like to redesign the whole alias routine such that "updates" are never needed. Where aliases aren't "maintained",
// as stateful, but rather are treated as true references, but that is a great undertaking, so updates will serve in the meantime.
// TODO: Add test case to check when 'descendant' is an alias. I to fix this by resolving it. I suspect there are other places that suffer the same way.
auto Network::update_aliases( Uuid const& node )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );
        
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( exists( node ), error_code::common::uncategorized );

    auto const rnode = resolve( node );
    auto const db = KTRY( fetch_component< com::Database >() );

    for( auto const& id : fetch_aliased_ancestry( rnode ) )
    {
        auto const dsts = KTRY( db->fetch_alias_destinations( id ) );

        for( auto const& dst : dsts )
        {
            KTRY( update_alias( id, dst ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto Network::update_body( Uuid const&  node
                         , std::string const& contents )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", node );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( alias_store().resolve( node ) ) );
        })
    ;

    auto const db = KTRY( fetch_component< com::Database >() );

    KMAP_TRY( db->update_body( alias_store().resolve( node ), contents ) );

    rv = outcome::success();

    return rv;
}

auto Network::update_heading( Uuid const& id
                            , Heading const& heading )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", id );
        // KM_RESULT_PUSH_STR( "heading", heading );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    auto rv = KMAP_MAKE_RESULT( void );
    auto const db = KTRY( fetch_component< com::Database >() );
    auto const rid = alias_store().resolve( id );

    KTRY( db->update_heading( rid, heading ) );

    rv = outcome::success();

    return rv;
}

auto Network::update_title( Uuid const& id
                          , Title const& title )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "node", id );
        // KM_RESULT_PUSH_STR( "title", title );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( id ) );
            }
        })
    ;

    KMAP_ENSURE( exists( id ), error_code::network::invalid_node );

    auto const db = KTRY( fetch_component< com::Database >() );
    auto const rid = alias_store().resolve( id );

    KTRY( db->update_title( rid, title ) );

    #if !KMAP_NATIVE
    {
        auto const vnw = KTRY( fetch_component< com::VisualNetwork >() );
        if( vnw->exists( rid ) )
        {
            // TODO: Should rather fire event.
            KTRY( vnw->update_title( rid, title ) );
        }
    }
    #endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

// TODO: Ensure root exists.
/**
 * For most path operations, selected is required to be lineal to root.
 * When the root is specified - and not the true root - selected may be nonlineal.
 * To work around this case, this routine returns root, or otherwise the original selected.
 **/
auto Network::adjust_selected( Uuid const& root ) const
    -> Uuid
{
    auto const selected = selected_node();

    if( is_lineal( root, selected ) )
    {
        return selected;
    }
    else
    {
        return root;
    }
}

auto Network::is_sibling( Uuid const& n1
                        , Uuid const& n2 ) const
    -> bool
{
    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( exists( n1 ) );
            BC_ASSERT( exists( n2 ) );
        })
    ;

    if( auto const p1 = fetch_parent( n1 )
      ; p1 )
    {
        if( auto const p2 = fetch_parent( n2 )
          ; p2 )
        {
            if( p1.value() == p2.value() )
            {
                return true;
            }
        }
    }

    return false;
}

auto Network::travel_left()
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const selected = selected_node();

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        KTRY( select_node( parent.value() ) );
        rv = parent.value();
    }
    else
    {
        rv = selected;
    }

    return rv;
}

auto Network::travel_right()
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const selected = selected_node();
    auto const children = anchor::node( selected )
                        | view2::child
                        | view2::order
                        | act2::to_node_vec( km );

    if( !children.empty() )
    {
        auto const dst = mid( children );

        KTRY( select_node( dst ) );
        rv = dst;
    }
    else
    {
        rv = selected;
    }

    return rv;
}

auto Network::travel_up()
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const selected = selected_node();

    if( auto const above = fetch_above( selected )
      ; above )
    {
        KMAP_TRY( select_node( above.value() ) );
        rv = above.value();
    }
    else
    {
        rv = selected;
    }

    return rv;
}

auto Network::travel_down()
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const selected = selected_node();

    if( auto const below = fetch_below( selected )
      ; below )
    {
        KMAP_TRY( select_node( below.value() ) );
        rv = below.value();
    }
    else
    {
        rv = selected;
    }

    return rv;
}

auto Network::travel_bottom()
    -> Uuid
{
    KM_RESULT_PROLOG();

    auto const selected = selected_node();
    auto rv = selected;
    auto& km = kmap_inst();

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        auto const children = anchor::node( parent.value() )
                            | view2::child
                            | view2::order
                            | act2::to_node_vec( km );

        KTRYE( select_node( children.back() ) );

        rv = children.back();
    }

    return rv;
}

auto Network::travel_top()
    -> Uuid
{
    KM_RESULT_PROLOG();

    auto const selected = selected_node();
    auto& km = kmap_inst();
    auto rv = selected;

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        auto const children = anchor::node( parent.value() )
                            | view2::child
                            | view2::order
                            | act2::to_node_vec( km );

        KTRYE( select_node( children.front() ) );

        rv = children.front();
    }

    return rv;
}

auto Network::is_child_internal( Uuid const& parent
                               , Uuid const& id ) const
    -> bool
{
    auto const cids = fetch_children( parent );

    return 0 != count( cids, id );
}

auto Network::is_child_internal( Uuid const& parent
                               , Heading const& heading ) const
    -> bool
{
    return bool{ fetch_child( parent, heading ) };
}

auto Network::is_lineal( Uuid const& ancestor
                       , Uuid const& descendant ) const
    -> bool
{
    return kmap::is_lineal( *this, ancestor, descendant );
}

auto Network::is_lineal( Uuid const& ancestor
                       , Heading const& descendant ) const
    -> bool
{
    return kmap::is_lineal( *this, ancestor, descendant );
}

auto Network::is_root( Uuid const& node ) const
    -> bool
{
    return kmap_inst().root_node_id() == node;
}

// TODO: Better named "is_root_alias"? To be consistent? Rather, `is_alias_root` to disambiguate from an alias to root.
// TODO: Frankly, should probably be a non-member util function.
/// Parent of alias is non-alias.
auto Network::is_top_alias( Uuid const& id ) const
    -> bool
{
    auto rv = false;

    if( auto const pid = fetch_parent( id )
      ; pid )
    {
        rv = is_alias( id ) && !is_alias( pid.value() ); // Top alias is one where the parent is non-alias and child is alias.
    }

    return rv;
}

SCENARIO( "Network::is_top_alias", "[com][network][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "nodes: 1, 2, 3" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const c3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

        THEN( "no top aliases" )
        {
            REQUIRE( !nw->is_top_alias( c1 ) );
            REQUIRE( !nw->is_top_alias( c2 ) );
            REQUIRE( !nw->is_top_alias( c3 ) );
        }

        GIVEN( "alias node: 2.1" )
        {
            auto const a21 = REQUIRE_TRY( nw->create_alias( c1, c2 ) );

            THEN( "only 2.1 is a top alias" )
            {
                REQUIRE( !nw->is_top_alias( c1 ) );
                REQUIRE( !nw->is_top_alias( c2 ) );
                REQUIRE( !nw->is_top_alias( c3 ) );
                REQUIRE( nw->is_top_alias( a21 ) );
            }

            GIVEN( "alias node: 3.2.1" )
            {
                auto const a32 = REQUIRE_TRY( nw->create_alias( c2, c3 ) );
                auto const a321 = REQUIRE_TRY( view::make( a32 ) | view::child | view::fetch_node( km ) );

                THEN( "only 2.1 and 3.2 are top aliases" )
                {
                    REQUIRE( !nw->is_top_alias( c1 ) );
                    REQUIRE( !nw->is_top_alias( c2 ) );
                    REQUIRE( !nw->is_top_alias( c3 ) );
                    REQUIRE( nw->is_top_alias( a21 ) );
                    REQUIRE( nw->is_top_alias( a32 ) );
                    REQUIRE( !nw->is_top_alias( a321 ) );
                }
            }
        }
    }
}

auto Network::fetch_child( Uuid const& parent 
                         , Heading const& heading ) const
   -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        // KM_RESULT_PUSH_NODE( "parent", parent );
        // KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    if( exists( parent ) )
    {
        for( auto const& e : fetch_children( parent ) )
        {
            if( heading == KTRY( fetch_heading( e ) ) )
            {
                rv = e;

                break;
            }
        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::node::parent_not_found, parent );
    }

    return rv;
}

// TODO: Utility, should be free func.
auto Network::fetch_visible_nodes_from( Uuid const& id
                                      , unsigned const& horizontal_max
                                      , unsigned const& vertical_max ) const
    -> std::vector< Uuid >
{ 
    auto rv = std::vector< Uuid >{};
    auto const& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( exists( e ) );
            }
            auto const s = UuidSet{ rv.begin(), rv.end() };
            BC_ASSERT( s.contains( id ) ); // Visible must include node itself.
            BC_ASSERT( rv.size() == s.size() ); // No duplicate nodes.
        })
    ;

    auto const lineage = anchor::node( id )
                       | view2::left_lineal
                       | view2::order
                       | act2::to_node_vec( km );
    auto const limited_lineage = lineage
                               | ranges::views::take_last( horizontal_max )
                               | ranges::to< std::vector >();

    auto push = [ &rv ]( auto const& r ) mutable
    {
        rv.insert( rv.end(), r.begin(), r.end() );
    };

    for( auto const& e : limited_lineage )
    {
        auto const siblings_ordered = anchor::node( e )
                                    | view2::sibling_incl
                                    | view2::order
                                    | act2::to_node_vec( km );
        auto const limited_siblings = select_median_range( siblings_ordered, e, vertical_max );

        if( e == id )
        {
            for( auto const& sibling : limited_siblings )
            {
                rv.emplace_back( sibling );

                if( sibling == id )
                {
                    auto const children_ordered = anchor::node( e )
                                                | view2::child
                                                | view2::order
                                                | act2::to_node_vec( km );
                    auto const limited_children = select_median_range( children_ordered, vertical_max );

                    push( limited_children );
                }
            }

        }
        else
        {
            push( limited_siblings );
        }
    }

    return rv;
}

SCENARIO( "Network::fetch_visible_nodes_from", "[com][network]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "/" )
    {
        THEN( "f( /, h:3, v:3 ): /" )
        {
            auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

            REQUIRE( nv == UuidVec{ root } );
        }

        GIVEN( "/1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "f( /, h:3, v:3 ): /, 1" )
            {
                auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                REQUIRE( nv == UuidVec{ root, n1 } );
            }
            THEN( "f( 1, h:2, v:2 ): /, 1" )
            {
                auto const nv = nw->fetch_visible_nodes_from( root, 2, 2 );

                REQUIRE( nv == UuidVec{ root, n1 } );
            }
            THEN( "f( 1, h:1, v:1 ): /, 1" )
            {
                auto const nv = nw->fetch_visible_nodes_from( root, 1, 1 );

                REQUIRE( nv == UuidVec{ root, n1 } );
            }

            GIVEN( "/2" )
            {
                auto const n2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

                THEN( "f( /, h:3, v:3 ): /, 1, 2" )
                {
                    auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                    REQUIRE( nv == UuidVec{ root, n1, n2 } );
                }
                THEN( "f( 1, h:3, v:3 ): /, 1, 2" )
                {
                    auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                    REQUIRE( nv == UuidVec{ root, n1, n2 } );
                }
                THEN( "f( 2, h:3, v:3 ): /, 1, 2" )
                {
                    auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                    REQUIRE( nv == UuidVec{ root, n1, n2 } );
                }

                GIVEN( "/3" )
                {
                    auto const n3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

                    THEN( "f( /, h:3, v:3 ): /, 1, 2, 3" )
                    {
                        auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                        REQUIRE( nv == UuidVec{ root, n1, n2, n3 } );
                    }
                    THEN( "f( 1, h:3, v:3 ): /, 1, 2, 3" )
                    {
                        auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                        REQUIRE( nv == UuidVec{ root, n1, n2, n3 } );
                    }
                    THEN( "f( 2, h:3, v:3 ): /, 1, 2, 3" )
                    {
                        auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                        REQUIRE( nv == UuidVec{ root, n1, n2, n3 } );
                    }
                    THEN( "f( 3, h:3, v:3 ): /, 1, 2, 3" )
                    {
                        auto const nv = nw->fetch_visible_nodes_from( root, 3, 3 );

                        REQUIRE( nv == UuidVec{ root, n1, n2, n3 } );
                    }
                }
            }
        }
    }
}

auto Network::create_child_aliases( Uuid const& src
                                  , Uuid const& child )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src", src );
        
    auto rv = KMAP_MAKE_RESULT( void );
    auto const& astore = alias_store();

    KMAP_ENSURE( exists( src ), error_code::common::uncategorized );
    KMAP_ENSURE( !is_alias( src ), error_code::common::uncategorized );
    KMAP_ENSURE( !is_alias( child ), error_code::common::uncategorized );
    KMAP_ENSURE( anchor::node( src ) | view2::child( child ) | act2::exists( kmap_inst() ), error_code::common::uncategorized );

    for( auto const dsts = astore.fetch_aliases( AliasItem::rsrc_type{ src } )
       ; auto const& dst : dsts )
    {
        KTRY( create_alias_leaf( child, dst ) );
    }

    rv = outcome::success();

    return rv;
}

auto Network::is_alias( Uuid const& node ) const
    -> bool
{
    return alias_store().is_alias( node );
}

auto Network::load_alias_leaf( Uuid const& src
                             , Uuid const& dst
                             , AliasLoadSet& all_aliases )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src", src );
        KM_RESULT_PUSH( "dst", dst );
    
    // fmt::print( "[load_alias_leaf][src]: {}\n", KTRY( fetch_heading( src ) ) );
    // fmt::print( "[load_alias_leaf][dst]: {}, parent: {}\n", KTRY( fetch_heading( dst ) ), KTRY( fetch_heading( KTRY( fetch_parent( dst ) ) ) ) );

    auto rv = result::make_result< void >();
    auto const rsrc = resolve( src );
    auto const alias_item = make_alias_item( src, rsrc, dst );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_parent( alias_item.alias() ).has_value() );
                BC_ASSERT( fetch_parent( alias_item.alias() ).value() == dst );
            }
        })
    ;

    KMAP_ENSURE( exists( rsrc ), error_code::network::invalid_node );
    KMAP_ENSURE( exists( dst ), error_code::network::invalid_node );

    auto const populate_src = [ & ]( auto const& ts ) -> Result< void >
    {
        // If "src has alias children", then "create those alias children", so the aliases exist when we need to reference them while populating another alias tree.
        auto const& dv = all_aliases.get< AliasItem::dst_type >();

        for( auto dst_er = dv.equal_range( AliasItem::dst_type{ ts } )
           ; dst_er.first != dst_er.second
           ; ++dst_er.first )
        {
            auto const& td = *dst_er.first;

            if( !td.loaded() )
            {
                KTRY( load_alias_leaf( td.rsrc().value(), td.dst().value(), all_aliases ) );
            }
        }

        return outcome::success();
    };
    auto const denote_as_loaded = [ &all_aliases ]( auto const& alias_id )
    {
        auto& av = all_aliases.get< AliasItem::alias_type >();
        auto const it = av.find( alias_id );

        if( it != av.end() ) // Root alias in this case.
        {
            av.modify( it, []( auto& item ){ item.loaded_flag = true; } );
        }
    };

    KTRY( populate_src( rsrc ) );
    denote_as_loaded( alias_item.alias() );
    KTRY( astore_.push_alias( alias_item ) );

    for( auto const rchildren = fetch_children( rsrc )
       ; auto const rchild : rchildren )
    {
        KTRY( load_alias_leaf( rchild, alias_item.alias(), all_aliases ) );
    }

    rv = outcome::success();

    return rv;
}

SCENARIO( "Network::is_alias", "[com][network][alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();

    GIVEN( "nodes: 1, 2, 3" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );
        auto const c3 = REQUIRE_TRY( nw->create_child( root, "3" ) );

        THEN( "regular nodes are not aliases" )
        {
            REQUIRE( !nw->is_alias( c1 ) );
            REQUIRE( !nw->is_alias( c2 ) );
            REQUIRE( !nw->is_alias( c3 ) );
        }

        GIVEN( "alias node: 2.1" )
        {
            auto const a21 = REQUIRE_TRY( nw->create_alias( c1, c2 ) );

            THEN( "only 2.1 is a alias" )
            {
                REQUIRE( !nw->is_alias( c1 ) );
                REQUIRE( !nw->is_alias( c2 ) );
                REQUIRE( !nw->is_alias( c3 ) );
                REQUIRE( nw->is_alias( a21 ) );
            }

            GIVEN( "alias node: 3.2.1" )
            {
                auto const a32 = REQUIRE_TRY( nw->create_alias( c2, c3 ) );
                auto const a321 = REQUIRE_TRY( view::make( a32 ) | view::child | view::fetch_node( km ) );

                THEN( "only 2.1, 3.2 and 3.2. are aliases" )
                {
                    REQUIRE( !nw->is_alias( c1 ) );
                    REQUIRE( !nw->is_alias( c2 ) );
                    REQUIRE( !nw->is_alias( c3 ) );
                    REQUIRE( nw->is_alias( a21 ) );
                    REQUIRE( nw->is_alias( a32 ) );
                    REQUIRE( nw->is_alias( a321 ) );
                }
            }
        }
    }
}

namespace {
namespace network_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Network
,   std::set({ "database"s, "root_node"s })
,   "network support"
);

} // namespace network_def
} // anonymous ns

} // namespace kmap::com
