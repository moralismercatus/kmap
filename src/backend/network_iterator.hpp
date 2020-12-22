
/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_BE_NETWORK_ITERATOR_HPP
#define KMAP_BE_NETWORK_ITERATOR_HPP

#include "../common.hpp"
#include "basic_network.hpp"

#include <boost/iterator/iterator_facade.hpp>
#include <range/v3/algorithm/find_if.hpp>

namespace kmap::backend {

template< typename Network >
class NetworkDfsIterator
    : public boost::iterator_facade< NetworkDfsIterator< Network >
                                   , Uuid const
                                   , boost::forward_traversal_tag >
{
public:
    NetworkDfsIterator() = default;
    explicit NetworkDfsIterator( Network const& nw )
        : nw_{ nw }
        , current_{ nw.root() }
        , siblings_{ std::set< Uuid >{ current_ } }
        , visited_{ std::set< Uuid >{ current_ } }
    {
    }

    auto increment()
        -> void
    {
        assert( nw_ );
        
        // 1. Select first child.
        // 2. Else: Select unvisited sibling.
        // 3. Else: Walk back up the ancestry searching for unvisited sibling.
        // 4. Else: We've reached the end.

        // 1. Select first child.
        if( auto const children = nw_->fetch_children( current_ ).value()
          ; !children.empty() )
        {
            current_ = *children.begin();
            siblings_ = children;

            visited_.emplace( current_ );
        }
        else
        {
            // 2. Else: Select unvisited sibling.
            auto const sibling = ranges::find_if( siblings_
                                                , [ & ]( auto const& e )
                                                {
                                                    return visited_.find( e ) == visited_.end();
                                                } );
            
            if( sibling != siblings_.end() )
            {
                current_ = *sibling;

                visited_.emplace( current_ );
            }
            else
            {
                // 3. Else: Walk back up the ancestry searching for unvisited sibling.
                if( auto next = fetch_next_ancestor( current_ )
                  ; next )
                {
                    current_ = next.value().first;
                    siblings_ = next.value().second;

                    visited_.emplace( current_ );
                }
                else
                {
                    // 4. Else: We've reached the end.
                    nw_ = nullopt;
                    current_ = {};
                    siblings_ = {};
                }
            }
            
        }
    }

    auto equal( NetworkDfsIterator const& other ) const
        -> bool
    {
        return nw_.has_value() == other.nw_.has_value()
            && current_ == other.current_;
    }

    auto dereference() const
        -> Uuid const& 
    {
        assert( nw_ );

        return current_;
    }

    auto begin() const
        -> NetworkDfsIterator
    {
        if( nw_ )
        {
            return NetworkDfsIterator{ nw_.value() };
        }
        else
        {
            return *this;
        }
    }

    auto end() const
        -> NetworkDfsIterator
    {
        return NetworkDfsIterator{};
    }

    auto cbegin() const
        -> NetworkDfsIterator
    {
        return begin();
    }
    
    auto cend() const
        -> NetworkDfsIterator
    {
        return end();
    }

protected:
    auto fetch_next_ancestor( Uuid const& node )
        -> Optional< std::pair< Uuid, std::set< Uuid > > >
    {
        assert( nw_ );

        auto rv = Optional< std::pair< Uuid, std::set< Uuid > > >{};
        auto parent = nw_->fetch_parent( node );

        while( parent && !rv )
        {
            parent = nw_->fetch_parent( parent.value() );

            if( parent )
            {
                auto const children = nw_->fetch_children( parent.value() ).value();
                auto const next = ranges::find_if( children 
                                                 , [ & ]( auto const& e )
                                                 {
                                                     return visited_.find( e ) == visited_.end();
                                                 } );
                
                if( next != children.end() )
                {
                    rv = std::pair{ *next, children };
                }
            }
        }

        return rv;
    }


private:
    Optional< Network const& > nw_ = {};
    Uuid current_ = {};
    std::set< Uuid > siblings_ = {};
    std::set< Uuid > visited_ = {};
};

} // namespace kmap::backend

#endif // KMAP_BE_NETWORK_ITERATOR_HPP