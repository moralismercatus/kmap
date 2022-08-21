/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/network/alias.hpp"

#include "attribute.hpp"
#include "com/database/db.hpp"
#include "com/network/network.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "test/util.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::com {

auto make_alias_id( Uuid const& alias_src
                  , Uuid const& alias_dst )
    -> Uuid
{
    return xor_ids( alias_src, alias_dst );
}

SCENARIO( "AliasSet basic ops" )
{
    auto aliases = AliasSet{};

    THEN( "empty set" )
    {
        REQUIRE( aliases.get< AliasItem::alias_type >().empty() );
        REQUIRE( aliases.get< AliasItem::src_type >().empty() );
        REQUIRE( aliases.get< AliasItem::dst_type >().empty() );
    }

    GIVEN( "alias item" )
    {
        auto const src = AliasItem::src_type{ gen_uuid() };
        auto const dst = AliasItem::dst_type{ gen_uuid() };
        auto const alias = make_alias_id( src.value(), dst.value() );

        aliases.emplace( AliasItem{ src, dst } );
                                  
        THEN( "duplicate alias insertion fails" )
        {
            auto const& v = aliases.get< AliasItem::alias_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( !aliases.emplace( AliasItem{ src, dst } ).second );
            REQUIRE( v.size() == 1 );
        }
        THEN( "items exist from alias view" )
        {
            auto const& v = aliases.get< AliasItem::alias_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( alias ) );

            auto const it = v.find( alias ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == src );
            REQUIRE( item.dst() == dst );
        }
        THEN( "items exist from src view" )
        {
            auto const& v = aliases.get< AliasItem::src_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( src ) );

            auto const it = v.find( src ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == src );
            REQUIRE( item.dst() == dst );
        }
        THEN( "items exist from dst view" )
        {
            auto const& v = aliases.get< AliasItem::dst_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( dst ) );

            auto const it = v.find( dst ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == src );
            REQUIRE( item.dst() == dst );
        }
    }
}

AliasStore::AliasStore( Kmap& km )
    : km_{ km }
{
}

// TODO: This fails if alias source is "/" - add a TC for this.
// TODO: Test when src is child of dst
auto AliasStore::create_alias( Uuid const& src
                             , Uuid const& dst )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nw = KTRY( km_.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( alias_set_.size() == alias_child_set_.size() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( alias_set_.size() == alias_child_set_.size() );

            if( rv )
            {
                BC_ASSERT( nw->exists( rv.value() ) );

                {
                    auto const& av = alias_child_set_.get< AliasChildItem::child_type >();

                    for( auto const& item : av )
                    {
                        BC_ASSERT( is_alias( item.child().value() ) );
                    }
                }
                {
                    auto const src_cnt = view::make( src )
                                       | view::desc 
                                       | view::count( km_ );
                    auto const rv_cnt = view::make( rv.value() )
                                      | view::desc
                                      | view::count( km_ );
                    BC_ASSERT( src_cnt == rv_cnt );
                }
            }
        })
    ;

    auto const rsrc = resolve( src );
    auto const rdst = resolve( dst );
    auto const alias_id = make_alias_id( rsrc, rdst );

    KMAP_ENSURE( src != km_.root_node_id(), error_code::network::invalid_node );
    KMAP_ENSURE( nw->exists( rsrc ), error_code::create_alias::src_not_found );
    KMAP_ENSURE( nw->exists( rdst ), error_code::create_alias::dst_not_found );
    KMAP_ENSURE( rsrc != rdst, error_code::create_alias::src_equals_dst );
    KMAP_ENSURE( !nw->is_lineal( rsrc, rdst ), error_code::create_alias::src_ancestor_of_dst );
    KMAP_ENSURE_MSG( !nw->exists( alias_id ), error_code::create_alias::alias_already_exists, KTRYE( absolute_path_flat( km_, rsrc ) ) );
    KMAP_ENSURE_MSG( !nw->is_child( rdst, KTRYE( nw->fetch_heading( rsrc ) ) ), error_code::create_node::duplicate_child_heading, KTRYE( nw->fetch_heading( rsrc ) ) );
    KMAP_ENSURE( !nw->is_child( rdst, rsrc ), error_code::network::invalid_node );

    {
        auto const db = KTRY( km_.fetch_component< com::Database >() );

        KTRY( db->push_alias( rsrc, rdst ) );

        KTRY( attr::push_order( km_, rdst, rsrc ) ); // Resolve src ID gets placed in ordering, rather than the alias ID.
    }

    KMAP_ENSURE( alias_set_.emplace( AliasItem{ AliasItem::src_type{ rsrc }, AliasItem::dst_type{ rdst } } ).second, error_code::network::invalid_node );
    KMAP_ENSURE( alias_child_set_.emplace( AliasChildItem{ AliasChildItem::parent_type{ rdst }, AliasChildItem::child_type{ alias_id } } ).second, error_code::network::invalid_node );

    for( auto const& e : nw->fetch_children( rsrc ) )
    {
        KTRY( create_internal_alias( e, alias_id ) );
    }

    rv = alias_id;

    return { rv };
}

auto AliasStore::create_internal_alias( Uuid const& src
                                      , Uuid const& dst )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nw = KTRY( km_.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( alias_set_.size() == alias_child_set_.size() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( alias_set_.size() == alias_child_set_.size() );
        })
    ;

    KMAP_ENSURE( nw->exists( resolve( src ) ), error_code::network::invalid_node );
    KMAP_ENSURE( nw->exists( dst ), error_code::network::invalid_node );

    auto const rsrc = resolve( src );
    auto const alias_id = make_alias_id( rsrc, dst );
    auto const alias_item = AliasItem{ AliasItem::src_type{ rsrc }, AliasItem::dst_type{ dst } };
    auto const ires =  alias_set_.emplace( alias_item ); 
    
    KMAP_ENSURE( ires.second, error_code::network::invalid_node );

    auto const aci = AliasChildItem{ AliasChildItem::parent_type{ dst }
                                   , AliasChildItem::child_type{ alias_id } };
                                   
    KMAP_ENSURE( alias_child_set_.emplace( aci ).second, error_code::network::invalid_node );


    BC_ASSERT( ires.first->alias() == alias_id );

    for( auto const& e : nw->fetch_children( rsrc ) )
    {
        KTRY( create_internal_alias( e, alias_id ) );
    }

    rv = alias_id;

    return rv;
}

auto AliasStore::erase_alias( Uuid const& id )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nw = KTRY( km_.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( id != km_.root_node_id() );
            BC_ASSERT( alias_set_.size() == alias_child_set_.size() );
        })
        BC_POST([ &
                , prev_as_size = BC_OLDOF( alias_set_.size() )
                , prev_acs_size = BC_OLDOF( alias_child_set_.size() ) ]
        {
            if( rv )
            {
                BC_ASSERT( !is_alias( id ) );
                BC_ASSERT( !nw->exists( id ) );
                BC_ASSERT( alias_set_.size() == alias_child_set_.size() );
                BC_ASSERT( alias_set_.size() + 1 == *prev_as_size );
                BC_ASSERT( alias_child_set_.size() + 1 == *prev_acs_size );
            }
        })
    ;

    KMAP_ENSURE_MSG( is_alias( id ), error_code::node::invalid_alias, to_string( id ) );

    auto const next_sel = KTRY( nw->fetch_next_selected_as_if_erased( id ) );
    auto const dst = KTRY( nw->fetch_parent( id ) );

    if( is_top_alias( id ) ) // Must precede erasing alias from the cache, as it is dependent on it.
    {
        auto const src = resolve( id );
        auto const db = KTRY( km_.fetch_component< com::Database >() );

        KTRY( db->erase_alias( src, dst ) );

        KTRY( attr::pop_order( km_, KTRY( nw->fetch_parent( id ) ), resolve( id ) ) ); // Resolved src is in order, rather than alias ID.
    }

    {
        auto&& av = alias_set_.get< AliasItem::alias_type >();
        auto const it = av.find( id );

        BC_ASSERT( it != av.end() );

        av.erase( it );
    }
    {
        auto&& av = alias_child_set_.get< AliasChildItem::child_type >();
        auto const er = av.equal_range( AliasChildItem::child_type{ id } );

        av.erase( er.first, er.second );
    }
    {
        auto&& av = alias_child_set_.get< AliasChildItem::parent_type >();
        auto const er = av.equal_range( AliasChildItem::parent_type{ id } );

        av.erase( er.first, er.second );
    }

    rv = next_sel;

    return rv;
}

auto AliasStore::erase_aliases_to( Uuid const& node )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( !has_alias( node ) );
        })
    ;

    auto const rnode = resolve( node );
    auto nodes = std::vector< Uuid >{};
    auto const& av = alias_set_.get< AliasItem::src_type >();
    auto const er = av.equal_range( AliasItem::src_type{ rnode } );
    
    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        nodes.emplace_back( it->alias() );
    }
    for( auto const& n : nodes )
    {
        KTRY( erase_alias( n ) );
    }

    rv = outcome::success();

    return rv;
}

auto AliasStore::fetch_aliased_ancestry( Uuid const& id ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( rv.size() <= ( view::make( id ) | view::ancestor | view::count( km_ ) ) );
        })
    ;

    auto const nw = KTRYE( km_.fetch_component< com::Network >() );
    auto child = id;
    auto parent = nw->fetch_parent( child );
    
    while( parent )
    {
        if( auto const aliases_from = fetch_aliases_to( child )
          ; aliases_from.size() > 0 )
        {
            rv.emplace_back( child );
        }

        child = parent.value();
        parent = nw->fetch_parent( child );
    }

    return rv;
}

auto AliasStore::fetch_alias_children( Uuid const& parent ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( alias_set_.size() == alias_child_set_.size() );

            for( auto const& e : rv )
            {
                BC_ASSERT( is_alias( e ) );
            }
        })
    ;

    auto const& av = alias_child_set_.get< AliasChildItem::parent_type >();
    auto const er = av.equal_range( AliasChildItem::parent_type{ parent } );

    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace_back( it->child().value() );
    }

    return rv;
}

auto AliasStore::fetch_aliases_to( Uuid const& src ) const
    -> std::vector< Uuid > // TODO: Result< UuidSet > instead?
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( is_alias( e ) );
            }
        })
    ;

    auto const& av = alias_set_.get< AliasItem::src_type >();
    auto const er = av.equal_range( AliasItem::src_type{ src } );
    
    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace_back( it->alias() );
    }

    return rv;
}

auto AliasStore::fetch_parent( Uuid const& child ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const& av = alias_child_set_.get< AliasChildItem::child_type >();
    auto const er = av.equal_range( AliasChildItem::child_type{ child } ); // TODO: equal_range, BC_ASSERT size == 1?

    if( std::distance( er.first, er.second ) == 1 )
    {
        rv = er.first->parent().value();
    }

    return rv;
}

auto AliasStore::has_alias( Uuid const& node ) const
    -> bool
{
    auto rv = false;

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !is_alias( node ) );
            }
        })
    ;
    // Every alias has a child_type entry (up to roots/topmost, where the parent is the real/dst node).
    auto const& av = alias_set_.get< AliasItem::src_type >();

    rv = av.contains( AliasItem::src_type{ node } );

    return rv;
}

SCENARIO( "aliased node has_alias()" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "alias_store" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = km.root_node_id();

    GIVEN( "two children" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

        WHEN( "second aliases the first" )
        {
            auto const a21 = REQUIRE_TRY( nw->alias_store().create_alias( c1, c2 ) );

            THEN( "first has_alias" )
            {
                REQUIRE( nw->alias_store().has_alias( c1 ) );
            }
            THEN( "alias itself doesn't has_alias()" )
            {
                REQUIRE( !nw->alias_store().has_alias( a21 ) );
            }
        }
    }
    GIVEN( "multiple descendants" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c11 = REQUIRE_TRY( nw->create_child( c1, "1" ) );
        auto const c111 = REQUIRE_TRY( nw->create_child( c11, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

        WHEN( "second aliases the first" )
        {
            auto const a21 = REQUIRE_TRY( nw->alias_store().create_alias( c1, c2 ) );
            auto const a211 = REQUIRE_TRY( nw->fetch_child( a21, "1" ) );
            auto const a2111 = REQUIRE_TRY( nw->fetch_child( a211, "1" ) );

            THEN( "first descs has_alias" )
            {
                REQUIRE( nw->alias_store().has_alias( c1 ) );
                REQUIRE( nw->alias_store().has_alias( c11 ) );
                REQUIRE( nw->alias_store().has_alias( c111 ) );
            }
            THEN( "aliases themselves don't has_alias()" )
            {
                REQUIRE( !nw->alias_store().has_alias( a21 ) );
                REQUIRE( !nw->alias_store().has_alias( a211 ) );
                REQUIRE( !nw->alias_store().has_alias( a2111 ) );
            }
        }
    }
}

auto AliasStore::is_alias( Uuid const& id ) const
    -> bool
{
    auto rv = false;

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( view::make( id ) | view::exists( km_ ) ); // Should never have an alias that doesn't exist as a node.
            }
        })
    ;

    auto const& av = alias_set_.get< AliasItem::alias_type >();

    rv = av.contains( id );

    return rv;
}

auto AliasStore::is_alias( Uuid const& src
                         , Uuid const& dst ) const
    -> bool
{
    return is_alias( make_alias_id( src, dst ) );
}

// TODO: Better named "is_root_alias"? To be consistent?
/// Parent of alias is non-alias.
auto AliasStore::is_top_alias( Uuid const& id ) const
    -> bool
{
    if( !is_alias( id ) )
    {
        return false;
    }

    auto const nw = KTRYE( km_.fetch_component< com::Network >() );
    auto const pid = nw->fetch_parent( id );

    return pid && !is_alias( pid.value() );
}

auto AliasStore::resolve( Uuid const& id ) const
    -> Uuid
{
    auto rv = Uuid{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( !is_alias( rv ) );

            if( is_alias( id ) )
            {
                BC_ASSERT( view::make( id ) | view::exists( km_ ) );
                BC_ASSERT( view::make( rv ) | view::exists( km_ ) );
            }
        })
    ;

    rv = id;

    if( is_alias( id ) )
    {
        auto const& av = alias_set_.get< AliasItem::alias_type >();
        
        if( auto const it = av.find( id )
          ; it != av.end() )
        {
            rv = it->src().value();
        }
    }

    return rv;
}


auto AliasStore::update_alias( Uuid const& from
                             , Uuid const& to )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( view::make( from ) | view::exists( km_ ) );
            BC_ASSERT( view::make( to ) | view::exists( km_ ) );
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
    auto const nw = KTRY( km_.fetch_component< com::Network >() );

    KTRY( nw->erase_node( make_alias_id( from, to ) ) );

    rv = create_alias( from, to );

    return rv;
}

// Note: I would like to redesign the whole alias routine such that "updates" are never needed. Where aliases aren't "maintained",
// as stateful, but rather are treated as true references, but that is a great undertaking, so updates will serve in the meantime.
// TODO: Add test case to check when 'descendant' is an alias. I to fix this by resolving it. I suspect there are other places that suffer the same way.
// TODO: I don't think this actually belongs here. Maybe in Network.
auto AliasStore::update_aliases( Uuid const& descendant )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( km_.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( nw->exists( descendant ) );
        })
    ;

    KMAP_ENSURE( nw->exists( descendant ), error_code::common::uncategorized );

    auto const rdescendant = resolve( descendant );
    auto const db = KTRY( km_.fetch_component< com::Database >() );

    for( auto const& id : fetch_aliased_ancestry( rdescendant ) )
    {
        auto const dsts = KTRY( db->fetch_alias_destinations( id ) );

        for( auto const& dst : dsts )
        {
            KMAP_TRY( update_alias( id, dst ) );
        }
    }

    rv = outcome::success();

    return rv;
}

// namespace {

// namespace binding
// {
//     using namespace emscripten;

//     EMSCRIPTEN_BINDINGS( kmap_filesystem )
//     {
//         function( "complete_filesystem_path", &kmap::com::complete_filesystem_path );
//         function( "fs_path_exists", &kmap::com::fs_path_exists );
//     }
// } // namespace binding
// } // namespace anon

} // namespace kmap::com
