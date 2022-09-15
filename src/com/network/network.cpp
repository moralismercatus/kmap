/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/network/network.hpp"

#include "attribute.hpp"
#include "com/database/db.hpp"
#include "com/database/root_node.hpp"
#include "com/network/visnetwork.hpp"
#include "error/result.hpp"
#include "kmap.hpp"
#include "path/act/order.hpp"
#include "path/act/take.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
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
    , astore_{ km }
{
}

auto Network::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const rn = KTRY( fetch_component< com::RootNode >() );

    selected_node_ = rn->root_node();
    
    rv = outcome::success();

    return rv;
}

auto Network::load()
    -> Result< void >
{
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
    auto rv = KMAP_MAKE_RESULT( void );
    auto const db = KTRY( fetch_component< com::Database >() );

    for( auto const& entry : db->fetch< db::AliasTable >() )
    {
        auto const src = entry.left().value();
        auto const dst = entry.right().value();

        KTRY( create_internal_alias( src, dst ) );
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
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( astore_.aliases().size() == astore_.child_aliases().size() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( astore_.aliases().size() == astore_.child_aliases().size() );

            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );

                {
                    auto const& av = astore_.child_aliases().get< AliasChildItem::child_type >();

                    for( auto const& item : av )
                    {
                        BC_ASSERT( astore_.is_alias( item.child().value() ) );
                    }
                }
                {
                    auto const src_cnt = view::make( src )
                                       | view::desc 
                                       | view::count( km );
                    auto const rv_cnt = view::make( rv.value() )
                                      | view::desc
                                      | view::count( km );
                    BC_ASSERT( src_cnt == rv_cnt );
                }
            }
        })
    ;

    auto const rsrc = resolve( src );
    auto const rdst = resolve( dst );
    auto const alias_id = make_alias_id( rsrc, rdst );

    KMAP_ENSURE( src != km.root_node_id(), error_code::network::invalid_node );
    KMAP_ENSURE( exists( rsrc ), error_code::create_alias::src_not_found );
    KMAP_ENSURE( exists( rdst ), error_code::create_alias::dst_not_found );
    KMAP_ENSURE( rsrc != rdst, error_code::create_alias::src_equals_dst );
    KMAP_ENSURE( !is_lineal( rsrc, rdst ), error_code::create_alias::src_ancestor_of_dst );
    KMAP_ENSURE_MSG( !exists( alias_id ), error_code::create_alias::alias_already_exists, KTRYE( absolute_path_flat( km, rsrc ) ) );
    KMAP_ENSURE_MSG( !is_child( rdst, KTRYE( fetch_heading( rsrc ) ) ), error_code::create_node::duplicate_child_heading, KTRYE( fetch_heading( rsrc ) ) );
    KMAP_ENSURE( !is_child( rdst, rsrc ), error_code::network::invalid_node );

    {
        auto const db = KTRY( km.fetch_component< com::Database >() );

        KTRY( db->push_alias( rsrc, rdst ) );

        KTRY( attr::push_order( km, rdst, rsrc ) ); // Resolve src ID gets placed in ordering, rather than the alias ID.
    }

    auto const pushed_id = KTRY( astore_.push_alias( rsrc, rdst ) );

    for( auto const& e : fetch_children( rsrc ) )
    {
        KTRY( create_internal_alias( e, alias_id ) );
    }

    BC_ASSERT( pushed_id == alias_id );

    rv = pushed_id;

    return rv;
}

SCENARIO( "AliasStore::create_alias", "[alias]" )
{
    // TODO: ...mucho grande thorough testing...
}

auto Network::create_child( Uuid const& parent
                          , Uuid const& child
                          , Heading const& heading
                          , Title const& title )
    -> Result< Uuid >
{
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

    auto const rpid = alias_store().resolve( parent );

    KTRY( create_child_internal( rpid, child, heading, title ) );

    if( !attr::is_in_attr_tree( km, child ) ) // TODO: What is the purpose of this check? And what's with it's name? Something feels half-baked about this...
    {
        KTRY( attr::push_genesis( km, child ) );
        KTRY( attr::push_order( km, rpid, child ) );
    }

    KTRY( update_aliases( parent ) );

    rv = child;

    return rv;
}

auto Network::create_child( Uuid const& parent
                          , Uuid const& child
                          , Heading const& heading )
    -> Result< Uuid >
{
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
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( create_child( parent
                           , gen_uuid()
                           , heading
                           , title ) );

    return rv;
}

auto Network::create_internal_alias( Uuid const& src
                                   , Uuid const& dst )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( astore_.aliases().size() == astore_.child_aliases().size() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( astore_.aliases().size() == astore_.child_aliases().size() );
        })
    ;

    KMAP_ENSURE( exists( resolve( src ) ), error_code::network::invalid_node );
    KMAP_ENSURE( exists( dst ), error_code::network::invalid_node );

    auto const rsrc = resolve( src );
    auto const alias_id = KTRY( astore_.push_alias( rsrc, dst ) );

    for( auto const& e : fetch_children( rsrc ) )
    {
        KTRY( create_internal_alias( e, alias_id ) );
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
    auto rv = KMAP_MAKE_RESULT( void );
    auto const db = KTRY( fetch_component< com::Database >() );

    KTRY( db->push_node( child ) );
    KTRY( db->push_heading( child, heading ) );
    KTRY( db->push_title( child, title ) );
    KTRY( db->push_child( parent, child ) );

    rv = outcome::success();

    return rv;
}

SCENARIO( "Network::create_child", "[com][nw]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "root_node" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const rn = REQUIRE_TRY( km.fetch_component< com::RootNode >() );

    GIVEN( "root" )
    {
        auto const root = rn->root_node();

        THEN( "create child with invalid heading fails" )
        {
            REQUIRE( !nw->create_child( root, "%" ) );
        }

        WHEN( "create child with heading" )
        {
            auto const cid = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "result is only child of root" )
            {
                auto const children = nw->fetch_children( root );

                REQUIRE( children.size() == 1 );
                REQUIRE( cid == *children.begin() );
            }
        }
    }
}

auto Network::erase_node_internal( Uuid const& id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
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
    KMAP_ENSURE( id != km.root_node_id(), error_code::network::invalid_node );

    // Delete children.
    for( auto const children = view::make( id )
                             | view::child
                             | view::to_node_set( km )
                             | act::order( km )
       ; auto const& e : children | views::reverse ) // Not necessary to erase in reverse order, but it seems like a reasonable requirement (FILO)
    {
        KMAP_TRY( erase_node_internal( e ) );
    }

    KTRY( erase_node_leaf( id ) );

    rv = outcome::success();

    return rv;
}

auto Network::erase_node_leaf( Uuid const& id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
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
    KMAP_ENSURE( id != km.root_node_id(), error_code::network::invalid_node );
    KMAP_ENSURE( fetch_children( id ).empty(), error_code::network::invalid_node );

    auto const db = KTRY( fetch_component< com::Database >() );

    // Delete node.
    if( alias_store().is_alias( id ) )
    {
        KMAP_TRY( alias_store().erase_alias( id ) );
    }
    else
    {
        if( alias_store().has_alias( id ) )
        {
            KTRY( alias_store().erase_aliases_to( id ) );
        }

        if( !attr::is_in_attr_tree( km, id ) )
        {
            auto const parent = KTRY( fetch_parent( id ) );

            KTRY( attr::pop_order( km, parent, id ) );

            // TODO: What does this get us vs. erase_all, and why both called?
            if( auto const at = fetch_attr_node( id )
              ; at )
            {
                KTRY( erase_attr( at.value() ) );
            }
        }

        db->erase_all( id );
    }

    rv = outcome::success();

    return rv;
}

auto Network::erase_attr( Uuid const& id )
    -> Result< Uuid >
{
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
        KMAP_TRY( erase_node_internal( child ) );
    }

    auto const db = KTRY( fetch_component< com::Database >() );
    auto const parent = db->fetch_attr_owner( id );

    db->erase_all( id );

    rv = parent;

    return rv;
}


// TODO: impl. body belongs in path.cpp
auto Network::distance( Uuid const& ancestor
                      , Uuid const& descendant ) const
    -> uint32_t
{
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
    KMAP_ENSURE( id != km.root_node_id(), error_code::node::is_root );
    KMAP_ENSURE( !alias_store().is_alias( id ) || is_top_alias( id ), error_code::node::is_nontoplevel_alias );

    auto const selected = selected_node();
    auto next_selected = selected;

    if( id == selected
     || is_ancestor( *this, id, selected ) )
    {
        auto const next_sel = KTRY( fetch_next_selected_as_if_erased( id ) );

        KTRY( select_node( next_sel ) );

        next_selected = next_sel;
    }
    else
    {
        next_selected = selected; // If not deleting selected, just return selected.
    }

    KTRY( erase_node_internal( id ) );

    rv = next_selected;

    // TODO: fire_event( { "map", "verb.post", "verb.erased", "node" }, std::any< payload:id >? ) // after checks, but before anything is done? Or before checks, in case handler modifies something?

    return rv;
}

SCENARIO( "erase_node erases attributes", "[nw][iface]" )
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

    GIVEN( "node" )
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
    }
}

auto Network::exists( Uuid const& id ) const
    -> bool
{
    auto const db = KTRYE( fetch_component< com::Database >() );

    return db->node_exists( alias_store().resolve( id ) );
}

auto Network::exists( Heading const& heading ) const
    -> bool
{
    return view::abs_root
         | view::desc( heading )
         | view::exists( kmap_inst() );
}


auto Network::fetch_above( Uuid const& node ) const
    -> Result< Uuid >
{
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
    
    if( auto const children = view::make( node )
                            | view::sibling_incl
                            | view::to_node_set( km )
                            | act::order( km )
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

SCENARIO( "Network::fetch_above", "[com][nw]" )
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
    
    if( auto const children = view::make( node )
                            | view::sibling_incl
                            | view::to_node_set( km )
                            | act::order( km )
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

SCENARIO( "Network::fetch_below", "[com][nw]" )
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
        if( auto const aliases_from = astore_.fetch_aliases_to( child )
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
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const db = KTRY( fetch_component< com::Database >() );

    rv = KTRY( db->fetch_attr( alias_store().resolve( id ) ) ); // TODO: resolve( id ) is under debate. Should resolving aliases be done implicitly, or required explicitly? Here, it is implicit.

    return rv;
}

auto Network::fetch_body( Uuid const& node ) const
    -> Result< std::string >
{
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
    auto rv = kmap::UuidSet{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // BC_ASSERT( alias_set_.size() == alias_child_set_.size() );

            for( auto const& e : rv )
            {
                BC_ASSERT( exists( e ) );
            }
        })
    ;

    KMAP_ENSURE_EXCEPT( exists( parent ) ); // TODO: Replace with KM_ENSURE( exists( parent ) );
    KMAP_ENSURE_EXCEPT( fetch_heading( parent ) ); // TODO: Replace with KM_ENSURE( exists( parent ) );

    auto const db_children = [ & ]
    {
        if( alias_store().is_alias( parent ) )
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
}

auto Network::fetch_heading( Uuid const& node ) const
    -> Result< Heading >
{
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
    auto const db = KTRYE( fetch_component< com::Database >() );

    return db->fetch_genesis_time( id );
}

auto Network::fetch_nodes( Heading const& heading ) const
    -> UuidSet
{
    auto const db = KTRYE( fetch_component< com::Database >() );

    return db->fetch_nodes( heading );
}

auto Network::fetch_ordering_position( Uuid const& node ) const
    -> Result< uint32_t >
{
    using Map = std::vector< std::pair< Uuid, uint64_t > >;

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

SCENARIO( "fetch_ordering_position", "[nw][iface][order]" )
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
            {
                auto const pa31 = REQUIRE_TRY( nw->fetch_parent( a31 ) );
                auto const porder = REQUIRE_TRY( view::make( pa31 )
                                        | view::resolve
                                        | view::attr
                                        | view::child( "order" )
                                        | view::fetch_node( kmap ) );
                auto const body = REQUIRE_TRY( nw->fetch_body( porder ) );
                fmt::print( "a21 id: {}\n", to_string( a21 ) );
                fmt::print( "resolve( a21 ) id: {}\n", to_string( nw->resolve( a21 ) ) );
                fmt::print( "a31 id: {}\n", to_string( a31 ) );
                fmt::print( "resolve( a31 ) id: {}\n", to_string( nw->resolve( a31 ) ) );
                fmt::print( "pa31 order.body: {}\n", body );
            }

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

auto Network::fetch_parent( Uuid const& child ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT_EC( Uuid, error_code::network::invalid_parent );
    auto const db = KTRY( fetch_component< com::Database >() );

    if( auto const pid = db->fetch_parent( child )
      ; pid )
    {
        rv = pid.value();
    }
    else if( alias_store().is_alias( child ) )
    {
        rv = KTRY( alias_store().fetch_parent( child ) );
    }

    return rv;
}

auto Network::fetch_title( Uuid const& id ) const
    -> Result< Title >
{
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

SCENARIO( "Network::fetch_title", "[com][nw]" )
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
    return kmap::move_body( *this, src, dst );
}

// TODO: Should this ensure that if the moved node is the selected node, it updates the selected as well? I believe this is only relevant for the alias case.
//       In essence, this can invalidate IDs for aliases.
auto Network::move_node( Uuid const& from
                       , Uuid const& to )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ &
                , from_alias = alias_store().is_alias( from ) ]
        {
            if( rv )
            {
                if( !from_alias )
                {
                    BC_ASSERT( is_child( to, from ) );
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
    else if( alias_store().is_alias( from ) )
    {
        auto const rfrom = resolve( from );

        KTRY( alias_store().erase_alias( from ) );

        auto const alias = KTRY( create_alias( rfrom, to ) );

        rv = alias;
    }
    else
    {
        auto const db = KTRY( fetch_component< com::Database >() );

        // TODO: It's important to have a fail-safe for this routine. If remove
        // succeeds, but, for some reason, add fails, the remove should revert (or
        // vice-versa), otherwise we're at risk of having a dangling node.
        db->erase_child( from_parent, from );
        KTRY( db->push_child( to, from ) );

        rv = to;
    }

    if( rv )
    {
        // TODO: Rather than remove from the network here, place the burden of responsibility on Kmap::select_node
        //       This represents a corner-case for the present impl. of Kmap::select_node, in that the visible_nodes
        //       will rightly return "from", but since it hasn't left the network, but just moved, the network will not recreate it,
        //       meaning it will not be placed in the correct place.
        auto const nw = KTRY( fetch_component< com::VisualNetwork >() );

        if( nw->exists( from ) )
        {
            KTRY( nw->remove_edge( from_parent, from ) );

            if( nw->exists( to ) )
            {
                KTRY( nw->add_edge( to, from ) );
            }
        }
    }

    return rv;
}

// Note: 'children' should be unresolved. TODO: Actually, I suspect this is only a requirement for the precondition, which could always resolve all IDs. To confirm. I believe, when it comes to ordering, the resolved node is the only one used.
auto Network::reorder_children( Uuid const& parent
                              , std::vector< Uuid > const& children )
    -> Result< void >
{
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
auto Network::root_node()
    -> Uuid
{
    auto const rn = KTRYE( fetch_component< com::RootNode >() );

    return rn->root_node();
}

auto Network::swap_nodes( Uuid const& t1
                        , Uuid const& t2 )
    -> Result< UuidPair >
{
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

SCENARIO( "swap two sibling aliases", "[nw][iface][swap_nodes][order]" )
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
            KMAP_LOG_LINE();
        auto const a32 = REQUIRE_TRY( nw->create_alias( c2, c3 ) );
            KMAP_LOG_LINE();

        THEN( "aliases are ordered properly" )
        {
            auto const a31_op = REQUIRE_TRY( nw->fetch_ordering_position( a31 ) );
            auto const a32_op = REQUIRE_TRY( nw->fetch_ordering_position( a32 ) );
            REQUIRE( a31_op == 0 );
            REQUIRE( a32_op == 1 );
        }

        WHEN( "swap aliases" )
        {
            REQUIRE_RES( print_tree( kmap, root ) );
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
    auto rv = error::make_result< Uuid >( kmap_inst(), id );

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

    auto const estore = KTRY( fetch_component< com::EventStore >() );
    auto const prev_selected = selected_node();
    selected_node_ = id;

    KTRY( estore->fire_event( { "subject.kmap", "verb.selected", "object.node" } ) );

    // TODO: breadcrumb should have its own handler listening for the fired event.
    // auto id_abs_path = absolute_path_uuid( id );
    // auto breadcrumb_nodes = id_abs_path
    //                       | views::drop_last( 1 )
    //                       | to< UuidVec >();

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
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( fetch_siblings_inclusive( km, id ).size() > pos );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_ordering_position( id ) );
                BC_ASSERT( fetch_ordering_position( id ).value() == pos );
            }
        })
    ;

    if( auto siblings = ( view::make( id ) 
                        | view::parent
                        | view::child
                        | view::to_node_set( km ) 
                        | act::order( km ) )
      ; siblings.size() > pos )
    {
        auto const parent = KTRY( fetch_parent( id ) );
        auto const it = find( siblings, id );

        BC_ASSERT( it != end( siblings) );

        std::iter_swap( it, begin( siblings ) + pos );

KMAP_LOG_LINE();
        KTRY( reorder_children( parent, siblings ) );
KMAP_LOG_LINE();

        rv = outcome::success();
    }

    return rv;
}

auto Network::update_alias( Uuid const& from
                          , Uuid const& to )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( view::make( from ) | view::exists( km ) );
            BC_ASSERT( view::make( to ) | view::exists( km ) );
        })
    ;

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
// TODO: I don't think this actually belongs here. Maybe in Network.
auto Network::update_aliases( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( exists( node ), error_code::common::uncategorized );

    auto const rnode = astore_.resolve( node );
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

// TODO: Am I crazy or doesn't id need to be resolved?
auto Network::update_body( Uuid const&  node
                         , std::string const& contents )
    -> Result< void >
{
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
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    auto rv = KMAP_MAKE_RESULT( void );
    auto const db = KTRY( fetch_component< com::Database >() );
    auto const vnw = KTRY( fetch_component< com::VisualNetwork >() );
    auto const rid = alias_store().resolve( id );

    KTRY( db->update_title( rid, title ) );

    if( exists( rid ) )
    {
        // TODO: should rather fire event.
        vnw->update_title( rid, title );
    }

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

auto Network::are_siblings( Uuid const& n1
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
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const selected = selected_node();

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        KMAP_TRY( select_node( parent.value() ) );
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
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const selected = selected_node();
    auto const children = view::make( selected )
                        | view::child
                        | view::to_node_set( km )
                        | act::order( km );

    if( !children.empty() )
    {
        auto const dst = mid( children );

        KMAP_TRY( select_node( dst ) );
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
    auto const selected = selected_node();
    auto rv = selected;
    auto& km = kmap_inst();

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        auto const children = view::make( parent.value() )
                            | view::child
                            | view::to_node_set( km )
                            | act::order( km );

        KTRYE( select_node( children.back() ) );

        rv = children.back();
    }

    return rv;
}

auto Network::travel_top()
    -> Uuid
{
    auto const selected = selected_node();
    auto& km = kmap_inst();
    auto rv = selected;

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        auto const children = view::make( parent.value() )
                            | view::child
                            | view::to_node_set( km )
                            | act::order( km );

        select_node( children.front() ).value();

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

// TODO: Better named "is_root_alias"? To be consistent?
/// Parent of alias is non-alias.
auto Network::is_top_alias( Uuid const& id ) const
    -> bool
{
    if( !astore_.is_alias( id ) )
    {
        return false;
    }

    auto const pid = fetch_parent( id );

    return pid && !astore_.is_alias( pid.value() );
}


auto Network::fetch_child( Uuid const& parent 
                         , Heading const& heading ) const
   -> Result< Uuid >
{
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
            if( heading == KMAP_TRY( fetch_heading( e ) ) )
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

// auto Kmap::fetch_children( Uuid const& root
//                          , Heading const& parent ) const
//     -> kmap::UuidSet
// { 
//     auto rv = kmap::UuidSet{};

//     if( auto const pn = fetch_descendant( root, parent )
//       ; pn )
//     {
//         rv = fetch_children( pn.value() );
//     }

//     return rv;
// }

auto Network::fetch_visible_nodes_from( Uuid const& id ) const
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
            BC_ASSERT( rv.size() == s.size() );
        })
    ;

    auto const lineage = view::make( id )
                       | view::lineage/*( view::count( 5 ) ), rather than take_last( N )*/
                       | view::to_node_set( km )
                       | act::order( km );
    auto const limited_lineage = lineage
                               | ranges::views::take_last( 5 ) // TODO: This max should be drawn from options.
                               | ranges::to< std::vector >();
    auto const lineage_set = limited_lineage | to< std::set< Uuid > >();

    BC_ASSERT( !lineage.empty() );

    rv.emplace_back( lineage[ 0 ] );

    for( auto const& e : lineage )
    {
        if( auto const all_ordered = view::make( e )
                                   | view::child
                                   | view::to_node_set( km )
                                   | act::order( km )
          ; all_ordered.size() > 0 )
        {
            auto const all_ordered_set = all_ordered | to< std::set< Uuid > >();
            auto const sub_ordered = [ & ]() -> std::vector< Uuid >
            {
                auto const range_size = 10u;

                if( auto const intersect = views::set_intersection( all_ordered_set, lineage_set )
                  ; begin( intersect ) != end( intersect ) )
                {
                    // TODO: BC_ASSERT( distance( intersect ) == 1 );

                    auto const lineal_child = *begin( intersect );

                    return select_median_range( all_ordered
                                              , lineal_child
                                              , range_size );
                }
                else
                {
                    return select_median_range( all_ordered
                                              , range_size );
                }

                return {};
            }();

            rv.insert( rv.end()
                     , sub_ordered.begin()
                     , sub_ordered.end() );
        }
    }

    return rv;
}

auto Network::is_alias( Uuid const& node ) const
    -> bool
{
    return alias_store().is_alias( node );
}

namespace binding {

using namespace emscripten;

struct Network
{
    Kmap& kmap_;

    Network( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto create_child( Uuid const& parent
                     , std::string const& title )
        -> kmap::binding::Result< Uuid >
    {
        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->create_child( parent, format_heading( title ), title );
    }
    auto fetch_parent( Uuid const& node )
        -> kmap::binding::Result< Uuid >
    {
        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->fetch_parent( node );
    }
    auto selected_node()
        -> Uuid
    {
        return KTRYE( kmap_.fetch_component< com::Network >() )->selected_node();
    }
    auto select_node( Uuid const& node )
        -> kmap::binding::Result< Uuid >
    {
        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->select_node( node );
    }
};

auto network()
    -> binding::Network
{
    return binding::Network{ Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_network )
{
    function( "network", &kmap::com::binding::network );
    class_< kmap::com::binding::Network >( "Network" )
        .function( "create_child", &kmap::com::binding::Network::create_child )
        .function( "fetch_parent", &kmap::com::binding::Network::fetch_parent )
        .function( "selected_node", &kmap::com::binding::Network::selected_node )
        .function( "select_node", &kmap::com::binding::Network::select_node )
        ;
}

} // namespace binding

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
