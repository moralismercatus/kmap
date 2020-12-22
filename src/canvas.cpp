#include "canvas.hpp"

#include "contract.hpp"
#include "error/node_manip.hpp"
#include "error/master.hpp"
#include "utility.hpp"

#include <emscripten.h>
#include <emscripten/val.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/split.hpp>

using namespace ranges;

namespace kmap
{

auto set_fwd_breadcrumb( std::string_view const text )
    -> bool
{
    using emscripten::val;

    auto rv = bool{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "fwd_breadcrumb" ).isUndefined() );
        })
    ;

    if( auto fwd_breadcrumb = val::global( "fwd_breadcrumb" )
      ; fwd_breadcrumb.as< bool >() )
    {
        fwd_breadcrumb.set( "value"
                          , std::string{ text } );

        rv = true;
    }
    else
    {
        rv = false;
    }

    return rv;
}

Canvas::Canvas()
{
    subdivisions_.emplace( "canvas", Division{ Division::Direction::horizontal, 1.0 } );
}

// Returns true if subdiv was created or modified, otherwise false.
auto Canvas::subdivide( std::string_view const path
                      , Division const& subdiv )
    -> Result< bool >
{
    auto rv = KMAP_MAKE_RESULT( bool );

    KMAP_ENSURE( rv, !has_parent( path ), error_code::canvas::invalid_parent );
    KMAP_ENSURE( rv, !is_conflicting( path, subdiv ), error_code::canvas::conflicts );
    KMAP_ENSURE( rv, !is_valid( subdiv ), error_code::canvas::invalid_subdiv );

    if( exists( path ) )
    {
        if( auto& existing = subdivisions_[ std::string{ path } ]
          ; existing.direction != subdiv.direction
         && existing.line != subdiv.line )
        {
            existing = subdiv;

            rv = true;
        }
    }
    else
    {
        subdivisions_.emplace( path, subdiv );
        groups_.emplace( fetch_parent_path( path ).value()
                       , subdiv );

        rv = true;
    }

    return rv;
}

auto Canvas::merge( std::string_view const path )
    -> bool
{
    auto rv = false;

    if( auto const it = subdivisions_.find( std::string{ path } )
      ; it != subdivisions_.end() )
    {
        subdivisions_.erase( it );

        rv = true;
    }

    return rv;
}

auto Canvas::exists( std::string_view const subdivision ) const
    -> bool
{
    return subdivisions_.find( std::string{ subdivision } ) != subdivisions_.end();
}

auto Canvas::fetch_parent_path( std::string_view const& subdivision ) const
    -> Optional< std::string >
{
    auto rv = Optional< std::string >{};
    auto const split = subdivision
                      | views::split( '.' )
                      | to< StringVec >();

    if( split.size() > 0 )
    {
        auto parent_path = split
                         | views::drop_last( 1 )
                         | views::join( '.' )
                         | to< std::string >();

        if( auto const it = subdivisions_.find( parent_path )
          ; it != subdivisions_.end() )
        {
            rv = parent_path;
        }
    }

    return rv;
}


auto Canvas::fetch_parent( std::string_view const& subdivision ) const
    -> Optional< Division >
{
    auto rv = Optional< Division >{};

    if( auto const pp = fetch_parent_path( subdivision )
      ; pp )
    {
        rv = subdivisions_.find( pp.value() )->second;
    }

    return rv;
}

auto Canvas::has_parent( std::string_view const subdivision ) const
    -> bool
{
    return fetch_parent_path( subdivision ).has_value();
}

auto Canvas::is_conflicting( std::string_view const path
                           , Division const& subdivision ) const
    -> bool
{
    auto rv = false;

    if( auto const parent_path = fetch_parent_path( path )
      ; parent_path )
    {
        for( auto [ it, end ] = groups_.equal_range( parent_path.value() )
           ; it != end
           ; ++it )
        {
            if( it->second.direction == subdivision.direction
             && it->second.line == subdivision.line )
            {
                rv = true;

                break;
            }
        }
    } 

    return rv;
}

auto Canvas::is_valid( Division const& subdiv ) const
    -> bool
{
    return subdiv.line < 0.0 || subdiv.line > 1.0;
}

} // namespace kmap
