/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_NODE_VIEW_HPP
#define KMAP_NODE_VIEW_HPP

#include "common.hpp"
#include "lineage.hpp"

namespace kmap
{

class Kmap;

// TODO: A few additions I'd like to add to this iface.
//       * operator[] returns a proxy object (that may be implicitly convertible to Uuid, TBD). Such an object will have accessible members (fetch children, parent, etc.).
//       * Possibly making the view or its proxy return range v3 aware, so as to leverage that API.
//            * Hypothetical example: kmap.node_view( n ) | descend( "charlie.victor" ) | reverse | take( 2 ) // Takes last two children of '[n].charlie.victor'.  
//            * As a counter, I'm not sure "descend" is even view-compatible (as it would generate an intermediate vector), and std::vector/set are already view-enabled, so is there much point?
class NodeView
{
public:
    NodeView( Kmap const& kmap
            , Lineal const& root_selected );
    NodeView( Kmap const& kmap
            , Uuid const& root );
    NodeView( Kmap const& kmap );

    // It's worth considering whether this should return a Result< Uuid >, allowing for failures, and conciseness via KMAP_TRY().
    auto operator[]( Heading const& heading ) const
        -> Uuid; 

private:
    Kmap const& kmap_;
    Lineal const root_selected_;
};

} // namespace kmap

#endif // KMAP_NODE_FETCHER_HPP
