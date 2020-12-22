/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

#include "contract.hpp"
#include "db_cache.hpp"
#include "io.hpp"

#include <range/v3/range/conversion.hpp>

using namespace ranges;

namespace kmap
{

auto DbCache::flush()
    -> void
{
    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( data_.nodes.set.empty() );
            BC_ASSERT( data_.headings.set.empty() );
            BC_ASSERT( data_.titles.set.empty() );
            BC_ASSERT( data_.bodies.set.empty() );
            BC_ASSERT( data_.children.set.empty() );
            BC_ASSERT( data_.child_orderings.set.empty() );
            BC_ASSERT( data_.nodes.completed_set.empty() );
            BC_ASSERT( data_.headings.completed_set.empty() );
            BC_ASSERT( data_.titles.completed_set.empty() );
            BC_ASSERT( data_.bodies.completed_set.empty() );
            BC_ASSERT( data_.children.completed_set.empty() );
            BC_ASSERT( data_.child_orderings.set.empty() );
            BC_ASSERT( !data_.headings.set_complete );
            BC_ASSERT( !data_.titles.set_complete );
            BC_ASSERT( !data_.bodies.set_complete );
            BC_ASSERT( !data_.children.set_complete );
            BC_ASSERT( !data_.child_orderings.set_complete );
            BC_ASSERT( !data_.root );
        })
    ;

    data_ = DbCacheData{};
}

auto DbCache::insert_alias( Uuid const& src
                          , Uuid const& dst )
    -> void
{ 
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !data_.aliases.set_complete );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( data_.aliases.set.get< 0 >().count( std::make_pair( src, dst ) ) );
            BC_ASSERT( data_.aliases.completed_set.count( src ) );
        })
    ;
    
    data_.aliases.set.emplace( src
                             , dst );
    data_.aliases.completed_set.emplace( src );
}

auto DbCache::insert_node( Uuid const& node )
    -> void
{ 
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( node != Uuid{} );
            BC_ASSERT( !data_.nodes.set_complete );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( data_.nodes.set.count( node ) );
            BC_ASSERT( data_.nodes.completed_set.count( node ) );
        })
    ;
    
    data_.nodes.set.emplace( node );
    data_.nodes.completed_set.emplace( node );
}

auto DbCache::insert_heading( Uuid const& node
                            , Heading const& heading )
    -> void
{ 
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !data_.headings.set_complete );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( data_.headings.set.get< 0 >().count( std::make_pair( node, heading ) ) );
            BC_ASSERT( data_.headings.completed_set.count( node ) );
        })
    ;

    data_.headings.set.emplace( node
                              , heading );
    data_.headings.completed_set.emplace( node );
}

auto DbCache::insert_title( Uuid const& node
                          , Title const& title )
    -> void
{ 
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !data_.titles.set_complete );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( data_.titles.set.get< 0 >().count( std::make_pair( node, title ) ) );
            BC_ASSERT( data_.titles.completed_set.count( node ) );
        })
    ;

    data_.titles.set.emplace( node
                            , title );
    data_.titles.completed_set.emplace( node );
}

auto DbCache::insert_body( Uuid const& node
                         , std::string const& body )
    -> void
{ 
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // TODO: Why was this a precond?
            // BC_ASSERT( !data_.bodies.set_complete );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( data_.bodies.set.get< 0 >().count( std::make_pair( node, body ) ) );
            BC_ASSERT( data_.bodies.completed_set.count( node ) );
        })
    ;

    data_.bodies.set.emplace( node
                            , body );
    data_.bodies.completed_set.emplace( node );
}

auto DbCache::insert_children( Uuid const& parent 
                             , UuidSet const& children )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !data_.children.set_complete );
        })
        BC_POST([ & ]
        {
            for( auto const& c : children )
            {
                BC_ASSERT( data_.children.set.get< 0 >().count( std::make_pair( parent, c ) ) );
            }
            BC_ASSERT( data_.children.completed_set.count( parent ) );
        })
    ;

    for( auto const& c : children )
    {
        data_.children.set.emplace( parent
                                  , c );
    }

    data_.children.completed_set.emplace( parent );
}

auto DbCache::insert_child_orderings( Uuid const& parent 
                                    , std::string const& orderings )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !data_.child_orderings.set_complete );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( data_.child_orderings.set.get< 0 >().count( std::make_pair( parent, orderings ) ) );
            BC_ASSERT( data_.child_orderings.completed_set.count( parent ) );
        })
    ;

    data_.child_orderings.set.emplace( parent
                                     , orderings );
    data_.child_orderings.completed_set.emplace( parent );
}

auto DbCache::fetch_aliases()
    -> CacheItem< AliasSet > const&
{
    return data_.aliases;
}

auto DbCache::fetch_nodes() const
    -> CacheItem< NodeSet > const&
{
    return data_.nodes;
}

auto DbCache::fetch_headings() const
    -> CacheItem< HeadingSet > const&
{
    return data_.headings;
}

auto DbCache::fetch_titles() const
    -> CacheItem< TitleSet > const&
{
    return data_.titles;
}

auto DbCache::fetch_bodies() const
    -> CacheItem< BodySet > const&
{
    return data_.bodies;
}

auto DbCache::fetch_children() const
    -> CacheItem< ChildSet > const&
{
    return data_.children;
}

auto DbCache::fetch_child_orderings() const
    -> CacheItem< ChildOrderingsSet > const&
{
    return data_.child_orderings;
}

auto DbCache::set_root( Uuid const& id )
    -> Uuid
{
    data_.root = id;

    return id;
}

auto DbCache::is_root( Uuid const& id ) const
    -> bool
{
    return id == data_.root;
}

auto DbCache::mark_node_set_complete()
    -> void
{
    data_.nodes.set_complete = true;
}

auto DbCache::mark_heading_set_complete()
    -> void
{
    data_.bodies.set_complete = true;
}

} // namespace kmap
