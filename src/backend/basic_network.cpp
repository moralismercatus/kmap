/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "basic_network.hpp"
#include "../error/network.hpp"
#include "../error/master.hpp"
#include "../common.hpp"
#include "../utility.hpp"
#include "../io.hpp"
#include "util/result.hpp"
#include "kmap.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>

using namespace ranges;

namespace kmap::backend
{

BasicNetwork::BasicNetwork( Uuid const& root )
    : root_{ root }
{
}

auto BasicNetwork::child_exists( Uuid const& parent
                               , Uuid const& child ) const
    -> bool
{
    return children_.find( IdPair{ parent, child } ) != children_.end();
}

auto BasicNetwork::child_exists( Uuid const& child ) const
    -> bool
{
    return children_.get< 2 >().find( child ) != children_.get< 2 >().end();
}

auto BasicNetwork::child_iterator_cbegin() const
    -> IdIdsSet::const_iterator
{
    return children_.cbegin();
}

auto BasicNetwork::child_iterator_cend() const
    -> IdIdsSet::const_iterator
{
    return children_.cend();
}

auto BasicNetwork::create_child( Uuid const& parent
                               , Uuid const& child )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( exists( parent ), error_code::network::invalid_parent );
    KMAP_ENSURE( !exists( child ), error_code::network::child_already_exists );

    if( children_.insert( IdPair{ parent, child } ).second )
    {
        rv = child;
    }

    return rv;
}

auto BasicNetwork::delete_node( Uuid const& node )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", node );

    auto rv = result::make_result< Uuid >();

    KMAP_ENSURE( exists( node ), error_code::network::invalid_node );

    auto const children = KTRY( fetch_children( node ) );

    for( auto const& child : children )
    {
        KTRY( delete_node( child ) );
    }

    auto&& children_set = children_.get< 2 >();

    if( auto const it = children_set.find( node )
      ; it != children_set.end() )
    {
        children_set.erase( it );

        rv = node;
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::network::invalid_node );
    }

    return rv;
}

auto BasicNetwork::exists( Uuid const& node ) const
    -> bool
{
    return ( ( children_.get< 1 >().find( node ) != children_.get< 1 >().end() )
          || ( children_.get< 2 >().find( node ) != children_.get< 2 >().end() )
          || ( node == root() ) );
}

auto BasicNetwork::fetch_children( Uuid const& parent ) const
    -> Result< std::set< Uuid > >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( std::set< Uuid > );

    KMAP_ENSURE( exists( parent ), error_code::network::invalid_parent );

    auto set = std::set< Uuid >{};

    for( auto range = children_.get< 1 >().equal_range( parent )
       ; range.first != range.second
       ; ++range.first )
    {
        set.emplace( range.first->second );
    }

    io::print( "parent: {}\n", parent );
    for( auto const& n : set )
    {
        io::print( "{},\n", n );
    }
    io::print( "\n" );

    rv = set;

    return rv;
}

auto BasicNetwork::fetch_children() const
    -> IdIdsSet const&
{
    return children_;
}

auto BasicNetwork::fetch_parent( Uuid const& child ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( child_exists( child ), error_code::network::invalid_parent );

    rv = children_.get< 2 >().find( child )->first;

    return rv;
}

auto BasicNetwork::move_node( Uuid const& src
                            , Uuid const& dst )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "src", src );
        KM_RESULT_PUSH_NODE( "dst", dst );

    auto rv = result::make_result< void >();

    KMAP_ENSURE( exists( src ), error_code::network::invalid_node );
    KMAP_ENSURE( exists( dst ), error_code::network::invalid_node );
    KMAP_ENSURE( src != dst, error_code::network::invalid_node );
    KMAP_ENSURE( src != root(), error_code::network::invalid_node );

    KTRY( delete_node( src ) );
    KTRY( create_child( dst, src ) );
    
    rv = outcome::success();

    return rv;
}

auto BasicNetwork::nodes() const
    -> UuidSet
{
    auto children = fetch_children()
                  | views::values
                  | to< UuidSet >();

    children.emplace( root() );

    return children;
}

auto BasicNetwork::copy( Uuid const& parent
                       , BasicNetwork const& other
                       , Uuid const& other_root )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_NODE( "other_root", other_root );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( exists( parent ), error_code::network::invalid_parent );

    io::print( "copying {} into {}\n", to_uint64( other_root ).value(), to_uint64( parent ).value() );

    auto const copied_root = KTRY( create_child( parent, other_root ) );
    auto const other_children = KTRY( other.fetch_children( other_root ) );

    for( auto const& other_child : other_children )
    {
        KTRY( copy( copied_root
                  , other
                  , other_child ) );
    }

    rv = outcome::success();

    return rv;
}

auto BasicNetwork::copy_descendants( Uuid const& parent
                                   , BasicNetwork const& other
                                   , Uuid const& other_root )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_NODE( "other_root", other_root );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const other_children = KMAP_TRY( other.fetch_children( other_root ) );

    for( auto const& other_child : other_children )
    {
        KMAP_TRY( copy( parent
                      , other
                      , other_child ) );
    }

    rv = outcome::success();

    return rv;
}

auto BasicNetwork::root() const
    -> Uuid
{
    return root_;
}

auto fetch_leaves( BasicNetwork const& network
                 , Uuid const& root )
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );

    auto rv = KMAP_MAKE_RESULT( UuidSet );
    auto leaves = UuidSet{};
    auto const children = KTRY( network.fetch_children( root ) );

    if( children.empty() )
    {
        leaves.emplace( root );
    }
    else
    {
        for( auto const& child : children )
        {
            auto const descendant_leaves = KTRY( fetch_leaves( network, child ) ); 

            leaves.insert( descendant_leaves.begin(), descendant_leaves.end() );
        }
    }

    rv = leaves;

    return rv;
}

auto fetch_lineage( BasicNetwork const& network
                  , Uuid const& node )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto lineage = UuidVec{};
    auto parent = network.fetch_parent( node );

    lineage.emplace_back( node );

    while( parent )
    {
        lineage.emplace_back( parent.value() );

        parent = network.fetch_parent( parent.value() );
    }

    rv = lineage
       | views::reverse
       | to< UuidVec >();

    return rv;
}

} // namespace kmap::backend