/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_HPP
#define KMAP_CANVAS_HPP

#include "common.hpp"

#include <string_view>
#include <memory>
#include <map>

namespace kmap
{
// TODO: A canvas represents what's rendered inside the window on screen. Thus do TextView, Editor, Network, breadcrumbs, CLI, belong here?

auto set_fwd_breadcrumb( std::string_view const text )
    -> bool;

struct Division
{
    enum class Direction
    {
        horizontal
    ,   vertical
    };

    Direction direction = {};
    float line = {};
};

class Canvas
{
public:
    Canvas();

    auto subdivide( std::string_view const path
                  , Division const& subdiv )
        -> Result< bool >;
    auto merge( std::string_view const path )
        -> bool;
    auto exists( std::string_view const subdivision ) const
        -> bool;
    auto fetch_parent_path( std::string_view const& subdivision ) const
        -> Optional< std::string >;
    auto fetch_parent( std::string_view const& subdivision ) const
        -> Optional< Division >;
    auto has_parent( std::string_view const subdivision ) const
        -> bool;
    auto is_conflicting( std::string_view const path
                       , Division const& subdivision ) const
        -> bool;
    auto is_valid( Division const& subdiv ) const
        -> bool;
    auto calculate_pixels( uint32_t const height
                         , uint32_t const width ) const
        -> std::map< std::string, Division >;
    auto calculate_pixels( uint32_t const height
                         , uint32_t const width
                         , std::string_view const path ) const
        -> Division; // TODO: I think Division is not enough, right? It needs a bound? Maybe even the now deleted "struct Bounds"?

private:
    std::map< std::string, Division > subdivisions_ = {};
    std::multimap< std::string, Division > groups_ = {}; // Convenience for accessing divisions by parent. TODO: Should be combined with subdivisions for boost.multi_index?
};

// Initial:
// auto canvas = Canvas{}; // creates default "canvas" subdivison that is the root of all others.
// canvas.subdivide( "canvas.breadcrumb", Division{ horizontal, 1.00 } );
// canvas.subdivide( "canvas.main", Division{ horizontal, 0.05 } );
// canvas.subdivide( "canvas.main.network", Division{ horizontal, 1.00 } );
// canvas.subdivide( "canvas.cli", Division{ horizontal, 0.95 } );

// `:view.body` 
// canvas.subdivide( "canvas.main.text", Division{ vertical, 0.50 } );
// canvas.subdivide( "canvas.main.text.text_view", Division{ horizontal, 1.00 } );

// `:edit.body`
// canvas.subdivide( "canvas.main.text", Division{ horizontal, 0.50 } );
// canvas.subdivide( "canvas.main.text_editor", Division{ horizontal, 1.00 } ); 
// canvas.subdivide( "canvas.main.text_editor.text_view", Division{ vertical, 0.50 } );

// exists( std::string_view const div ) 
// operator[]( std::string_view const div ) fetch division
// begin/end for iterating through all subdivisions. Just return begin/end to map< path, Division >

// `:view.timeline`
// canvas.merge( "canvas.main.network" ); Note:: Just deletes the internal subdivision, not the kmap::Network, nor the visjs::Network! Important so it can be reopened.
// canvas.subdivide( "canvas.main.timeline", Division{ horizontal, 1.0 } );

// Determines pixel sizes for each element, as mapped to individual paths, relative to absolute pixels of the canvas area.
// Suppose we needed to know the size of the "network", so we could render it in the correct place.
// auto const canvas_width = (root div).clientWidth; 
// auto const canvas_height = (root div).clientHeight; 
// ...assuming already set up desired Division...
// auto const relative_pixels = canvas.calculate_pixels( "canvas.main.network" );
// relative_pixels.first, relative_pixels.last (using top-bottom/left-right), describes where, in pixels, to place the element.
// I believe this demonstrates how to use div given a particular position: https://stackoverflow.com/questions/6802956/how-to-position-a-div-in-a-specific-coordinates
// Will need a bit of experimenting...
// https://www.w3schools.com/jsref/event_onresize.asp - use callback for "onresize" to recalculate Divisions when window changes size.

// Another note: Should be able to use this to tell whether a particular element (network, text_editor, timeline, etc) are visibile by querying if it has a subdivision.
// If not, it won't be visible.

} // namespace kmap

#endif // KMAP_CANVAS_HPP
