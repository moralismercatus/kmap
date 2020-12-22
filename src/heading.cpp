#include "heading.hpp"
#include "contract.hpp"

#include <range/v3/algorithm/find_if_not.hpp>

using namespace ranges;

namespace kmap
{

auto to_heading( std::string_view const s )
    -> Optional< Heading >
{
    auto rv = Optional< Heading >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_valid_heading( s ) );
            }
        })
    ;

    if( is_valid_heading( s ) )
    {
        rv = Heading{ s };
    }

    return rv;
}

auto to_heading_path( std::string_view const s )
    -> Optional< HeadingPath >
{
    auto rv = Optional< HeadingPath >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_valid_heading( s ) );
            }
        })
    ;

    // TODO: Some deliberation needed here...
    // How to represent or preserve backward and forward headings?
    // Is it important to preserve direction, or maintain only forwards?
    // I'd say it's important for CLI input, but otherwise, I may be able to maintain only FWD.
    // Another problem is how pass around paths like ",,,foo.bar"

    return rv;
}

auto is_valid_heading_char( char const c )
    -> bool
{
    return isalnum( c )
        || c == '_';
}

auto is_valid_heading( std::string_view const s )
    -> bool
{
    return end( s ) == find_if_not( s
                                  , is_valid_heading_char );
}

auto begin( Heading& heading )
    -> std::string::iterator
{ 
    return heading.begin();
}

auto begin( Heading const& heading )
    -> std::string::const_iterator
{
    return heading.cbegin();
}

auto end( Heading& heading )
    -> std::string::iterator
{
    return heading.end();
}

auto end( Heading const& heading )
    -> std::string::const_iterator
{
    return heading.cend();
}

} // namespace kmap
