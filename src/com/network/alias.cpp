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
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::com {

auto make_alias_id( Uuid const& alias_src
                  , Uuid const& alias_dst )
    -> Uuid
{
    return xor_ids( alias_src, alias_dst );
}

auto make_alias_item( Uuid const& top
                    , Uuid const& src
                    , Uuid const& rsrc
                    , Uuid const& dst )
    -> AliasItem
{
    return AliasItem{ .src_id = AliasItem::src_type{ src }
                    , .rsrc_id = AliasItem::rsrc_type{ rsrc }
                    , .dst_id = AliasItem::dst_type{ dst }
                    , .top_id = AliasItem::top_type{ top } };
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
        auto const alias_item = make_alias_item( make_alias_id( src_id, dst_id ), src_id, src_id, dst_id );
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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

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

auto AliasStore::fetch_alias_children( AliasItem::alias_type const& parent ) const
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

SCENARIO( "AliasStore::fetch_alias_children", "[alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = kmap::Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();
    auto const& astore = nw->alias_store();

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

        auto const check = [ & ]( auto const& parent, UuidSet const& children )
        {
            return astore.fetch_alias_children( parent ) == children;
        };

        THEN( "fac( /1 ) == {}" ) { REQUIRE( check( n1, {} ) ); }
        THEN( "fac( /2 ) == { /2.1 }" ) { REQUIRE( check( n2, { n21 } ) ); }
        THEN( "fac( /2.1 ) == {}" ) { REQUIRE( check( n21, {} ) ); }
        THEN( "fac( /3 ) == { /3.2 }" ) { REQUIRE( check( n3, { n32 } ) ); }
        THEN( "fac( /3.2 ) == { /3.2.1 }" ) { REQUIRE( check( n32, { n321 } ) ); }
        THEN( "fac( /3.2.1 ) == {}" ) { REQUIRE( check( n321, {} ) ); }
        THEN( "fac( /4 ) == { /4.3 }" ) { REQUIRE( check( n4, { n43 } ) ); }
        THEN( "fac( /4.3 ) == { /4.3.2 }" ) { REQUIRE( check( n43, { n432 } ) ); }
        THEN( "fac( /4.3.2 ) == { /4.3.2.1 }" ) { REQUIRE( check( n432, { n4321 } ) ); }
        THEN( "fac( /4.3.2.1 ) == {}" ) { REQUIRE( check( n4321, {} ) ); }
    }
}

auto AliasStore::fetch_aliases( AliasItem::rsrc_type const& rsrc ) const
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

    auto const& av = alias_set_.get< AliasItem::rsrc_type >();
    auto const er = av.equal_range( rsrc );
    
    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace( it->alias() );
    }

    return rv;
}

SCENARIO( "AliasStore::fetch_aliases", "[alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = kmap::Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();
    auto const& astore = nw->alias_store();

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

        auto const check = [ & ]( auto const& src, UuidSet const& aliases )
        {
            return astore.fetch_aliases( AliasItem::rsrc_type{ src } ) == aliases;
        };

        THEN( "fas( /1 ) == { /2.1, /3.2.1, /4.3.2.1 }" ) { REQUIRE( check( n1, { n21, n321, n4321 } ) ); }
        THEN( "fas( /2 ) == { /3.2, /4.3.2 }" ) { REQUIRE( check( n2, { n32, n432 } ) ); }
        THEN( "fas( /2.1 ) == {}" ) { REQUIRE( check( n21, {} ) ); }
        THEN( "fas( /3 ) == { /4.3 }" ) { REQUIRE( check( n3, { n43 } ) ); }
        THEN( "fas( /3.2 ) == {}" ) { REQUIRE( check( n32, {} ) ); }
        THEN( "fas( /3.2.1 ) == {}" ) { REQUIRE( check( n321, {} ) ); }
        THEN( "fas( /4 ) == {}" ) { REQUIRE( check( n4, {} ) ); }
        THEN( "fas( /4.3 ) == {}" ) { REQUIRE( check( n43, {} ) ); }
        THEN( "fas( /4.3.2 ) == {}" ) { REQUIRE( check( n432, {} ) ); }
        THEN( "fas( /4.3.2.1 ) == {}" ) { REQUIRE( check( n4321, {} ) ); }
    }
}

auto AliasStore::fetch_aliases( AliasItem::top_type const& top ) const
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

    auto const& av = alias_set_.get< AliasItem::top_type >();
    auto const er = av.equal_range( top );
    
    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace( it->alias() );
    }

    return rv;
}

auto AliasStore::fetch_dsts( AliasItem::rsrc_type const& rsrc ) const
    -> std::set< Uuid > 
{
    auto rv = std::set< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( is_alias( make_alias_id( rsrc.value(), e ) ) );
            }
        })
    ;

    auto const& av = alias_set_.get< AliasItem::rsrc_type >();
    auto const er = av.equal_range( rsrc );
    
    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace( it->dst().value() );
    }

    return rv;
}

SCENARIO( "AliasStore::fetch_dsts", "[alias]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = kmap::Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const root = nw->root_node();
    auto const& astore = nw->alias_store();

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

        auto const check = [ & ]( auto const& src, UuidSet const& dsts )
        {
            return astore.fetch_dsts( AliasItem::rsrc_type{ src } ) == dsts;
        };

        THEN( "fads( /1 ) == { /2.1, /3.2.1, /4.3.2.1 }" ) { REQUIRE( check( n1, { n2, n32, n432 } ) ); }
        THEN( "fads( /2 ) == { /3, /4.3 }" ) { REQUIRE( check( n2, { n3, n43 } ) ); }
        THEN( "fads( /2.1 ) == {}" ) { REQUIRE( check( n21, {} ) ); }
        THEN( "fads( /3 ) == { /4 }" ) { REQUIRE( check( n3, { n4 } ) ); }
        THEN( "fads( /3.2 ) == {}" ) { REQUIRE( check( n32, {} ) ); }
        THEN( "fads( /3.2.1 ) == {}" ) { REQUIRE( check( n321, {} ) ); }
        THEN( "fads( /4 ) == {}" ) { REQUIRE( check( n4, {} ) ); }
        THEN( "fads( /4.3 ) == {}" ) { REQUIRE( check( n43, {} ) ); }
        THEN( "fads( /4.3.2 ) == {}" ) { REQUIRE( check( n432, {} ) ); }
        THEN( "fads( /4.3.2.1 ) == {}" ) { REQUIRE( check( n4321, {} ) ); }
    }
}

auto AliasStore::fetch_entry( AliasItem::alias_type const& id ) const
    -> Result< AliasItem >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( AliasItem );
    auto const& av = alias_set_.get< AliasItem::alias_type >();
    auto const er = av.equal_range( id );

    if( std::distance( er.first, er.second ) == 1 )
    {
        rv = *( er.first );
    }
    else
    {
        // TODO: Inform rv what went wrong?
        fmt::print( "{} entries found for alias_id: {}\n", std::distance( er.first, er.second ), to_string( id ) );
    }

    return rv;
}

auto AliasStore::fetch_entries( AliasItem::rsrc_type const& rsrc ) const
    -> std::set< AliasItem > 
{
    auto rv = std::set< AliasItem >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( is_alias( e ) );
            }
        })
    ;

    auto const& av = alias_set_.get< AliasItem::rsrc_type >();
    auto const er = av.equal_range( rsrc );
    
    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace( *it );
    }

    return rv;
}

auto AliasStore::fetch_parent( AliasItem::alias_type const& child ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "child", child );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    
    rv = KTRY( fetch_entry( child ) ).dst().value();

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
    KM_RESULT_PROLOG();

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

} // namespace kmap::com
