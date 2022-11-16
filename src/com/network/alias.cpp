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

auto make_alias_item( Uuid const& src
                    , Uuid const& rsrc
                    , Uuid const& dst )
    ->AliasItem
{
    return AliasItem{ .src_id = AliasItem::src_type{ src }
                    , .rsrc_id = AliasItem::rsrc_type{ rsrc }
                    , .dst_id = AliasItem::dst_type{ dst } };
}

SCENARIO( "AliasSet basic ops", "[alias]" )
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
        auto const src_id = gen_uuid();
        auto const dst_id = gen_uuid();
        auto const alias_item = make_alias_item( src_id, src_id, dst_id );
        auto const alias = alias_item.alias();

        aliases.emplace( alias_item );
                                  
        THEN( "duplicate alias insertion fails" )
        {
            auto const& v = aliases.get< AliasItem::alias_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( !aliases.emplace( alias_item ).second );
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
            REQUIRE( item.src() == alias_item.src() );
            REQUIRE( item.dst() == alias_item.dst() );
        }
        THEN( "items exist from src view" )
        {
            auto const& v = aliases.get< AliasItem::src_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( alias_item.src() ) );

            auto const it = v.find( alias_item.src() ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == alias_item.src() );
            REQUIRE( item.dst() == alias_item.dst() );
        }
        THEN( "items exist from dst view" )
        {
            auto const& v = aliases.get< AliasItem::dst_type >();

            REQUIRE( v.size() == 1 );
            REQUIRE( v.contains( alias_item.dst() ) );

            auto const it = v.find( alias_item.dst() ); REQUIRE( it != v.end() );
            auto const item = *it;

            REQUIRE( item.alias() == alias );
            REQUIRE( item.src() == alias_item.src() );
            REQUIRE( item.dst() == alias_item.dst() );
        }
    }
}

auto AliasStore::erase_alias( Uuid const& id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_POST([ &
                , prev_as_size = BC_OLDOF( alias_set_.size() ) ]
        {
            if( rv )
            {
                BC_ASSERT( !is_alias( id ) );
                BC_ASSERT( alias_set_.size() < *prev_as_size );
            }
        })
    ;

    KMAP_ENSURE_MSG( is_alias( id ), error_code::node::invalid_alias, to_string( id ) );
    
    auto const child_items = [ & ]
    {
        auto cis = std::set< Uuid >{};
        auto const& dv = alias_set_.get< AliasItem::dst_type >();
        for( auto cits = dv.equal_range( AliasItem::dst_type{ id } )
           ; cits.first != cits.second
           ; ++cits.first )
        {
            cis.emplace( cits.first->alias() );
        }

        return cis;
    }();

    for( auto const& ci : child_items ) // TODO: Needs to be sorted by reverse order, so erasures are done in proper order (bottom to top).
    {
        KTRY( erase_alias( ci ) );
    }

    auto&& av = alias_set_.get< AliasItem::alias_type >();

    if( 1 == av.erase( id ) )
    {
        rv = outcome::success();
    }

    return rv;
}

// auto AliasStore::erase_alias_child( Uuid const& parent )
//     -> Result< void >
// {
//         fmt::print( "erase_alias_child( {} ): {}\n", to_string( parent ), KTRYE( absolute_path_flat( kmap::Singleton::instance(), parent ) ) );
//     auto rv = KMAP_MAKE_RESULT( void );

//     // KMAP_ENSURE( has_alias_child( parent ), error_code::common::uncategorized );

//     auto const children = [ & ] // Need copies of children, not iterators, so erasures of the underlying set doesn't invalidate iterators.
//     {
//         auto const& av = alias_child_set_.get< AliasChildItem::parent_type >();
//         auto cs = UuidSet{};
//         for( auto cr = av.equal_range( AliasChildItem::parent_type{ parent } )
//            ; cr.first != cr.second
//            ; ++cr.first )
//         {
//             cs.emplace( cr.first->child().value() );
//         }
//         return cs; // | act::ordered() TODO: This needs to erase aliases in the proper order, just like other nodes, regardless that they are aliases.
//     }();

//     for( auto const& child : children )
//     {
//         KTRY( erase_alias_child( child ) );
//     }

//     {
//         auto&& av = alias_set_.get< AliasItem::alias_type >();

//         av.erase( parent );
//     }
//     {
//         auto&& av = alias_child_set_.get< AliasChildItem::parent_type >();

//         av.erase( AliasChildItem::parent_type{ parent } );
//     }

//     // TODO: Don't we need to erase from alias_set as well? Or is it top alias v. the rest; the "children"?

//     rv = outcome::success();

//     return rv;
// }

auto AliasStore::aliases() const
    -> AliasSet const&
{
    return alias_set_;
}

auto AliasStore::fetch_alias_children( Uuid const& parent ) const
    -> std::set< Uuid >
{
    auto rv = std::set< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( is_alias( e ) );
            }
        })
    ;

    auto const& av = alias_set_.get< AliasItem::dst_type >();
    auto const er = av.equal_range( AliasItem::dst_type{ parent } );

    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace( it->alias() );
    }

    return rv;
}

auto AliasStore::fetch_aliases_dsts( Uuid const& src ) const
    -> std::set< Uuid > 
{
    auto rv = std::set< Uuid >{};

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
        rv.emplace( it->alias() );
    }

    return rv;
}

auto AliasStore::fetch_entry( Uuid const& id ) const
    -> Result< AliasItem >
{
    auto rv = KMAP_MAKE_RESULT( AliasItem );
    auto const& av = alias_set_.get< AliasItem::alias_type >();
    auto const er = av.equal_range( AliasItem::alias_type{ id } );

    if( std::distance( er.first, er.second ) == 1 )
    {
        rv = *( er.first );
    }
    else
    {
        fmt::print( "{} entries found for alias_id: {}\n", std::distance( er.first, er.second ), to_string( id ) );
    }

    return rv;
}

auto AliasStore::fetch_parent( Uuid const& child ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    
    rv = KTRY( fetch_entry( child ) ).dst().value();
    // auto const& av = alias_set_.get< AliasItem::alias_type >();
    // auto const er = av.equal_range( AliasItem::alias_type{ child } ); // TODO: equal_range, BC_ASSERT size == 1?

    // if( std::distance( er.first, er.second ) == 1 )
    // {
    //     rv = er.first->dst().value();
    // }

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

SCENARIO( "aliased node has_alias()", "[alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = km.root_node_id();

    GIVEN( "two children" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c2 = REQUIRE_TRY( nw->create_child( root, "2" ) );

        WHEN( "second aliases the first" )
        {
            auto const a21 = REQUIRE_TRY( nw->create_alias( c1, c2 ) );

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
            auto const a21 = REQUIRE_TRY( nw->create_alias( c1, c2 ) );
            auto const a211 = REQUIRE_TRY( nw->fetch_child( a21, "1" ) );
            auto const a2111 = REQUIRE_TRY( nw->fetch_child( a211, "1" ) );

            REQUIRE( nw->is_alias( a21 ) );
            REQUIRE( nw->is_alias( a211 ) );
            REQUIRE( nw->is_alias( a2111 ) );

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

// auto AliasStore::has_alias_child( Uuid const& node ) const
//     -> bool
// {
//     auto const& av = alias_child_set_.get< AliasChildItem::parent_type >();

//     return av.contains( AliasChildItem::parent_type{ node } );
// }

auto AliasStore::push_alias( AliasItem const& item )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_entry( item.alias() ).has_value() );
            }
        })
    ;
    // auto const alias_id = make_alias_id( src, dst );

    KMAP_ENSURE( alias_set_.emplace( item ).second, error_code::network::invalid_node );
    // KMAP_ENSURE( alias_child_set_.emplace( AliasChildItem{ AliasChildItem::parent_type{ dst }, AliasChildItem::child_type{ alias_id } } ).second, error_code::network::invalid_node );

    rv = item.alias();

    return rv;
}

auto AliasStore::is_alias( Uuid const& id ) const
    -> bool
{
    auto rv = false;

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // Removing this, as view::exists has a dependency on Network, which could represent a cyclic dependency if Network hasn't yet been fully initialized.
            // if( rv )
            // {
            //     BC_ASSERT( view::make( id ) | view::exists( km_ ) ); // Should never have an alias that doesn't exist as a node.
            // }
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

auto AliasStore::resolve( Uuid const& id ) const
    -> Uuid
{
    auto rv = Uuid{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( !is_alias( rv ) );

            // Can't do this. view::exists depends on com::Network, which depends on AliasStore, so cyclic.
            // if( is_alias( id ) )
            // {
            //     BC_ASSERT( view::make( id ) | view::exists( km_ ) );
            //     BC_ASSERT( view::make( rv ) | view::exists( km_ ) );
            // }
        })
    ;

    if( is_alias( id ) )
    {
        auto const& av = alias_set_.get< AliasItem::alias_type >();
        
        if( auto const it = av.find( id )
          ; it != av.end() )
        {
            rv = it->rsrc().value();
        }
    }
    else
    {
        rv = id;
    }

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