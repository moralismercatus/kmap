/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <util/markdown.hpp>

#include <common.hpp>
#include <js/iface.hpp>
#include <io.hpp>

#include <emscripten.h>
#include <emscripten/val.h>

namespace kmap {

auto markdown_to_html( std::string const& text )
    -> std::string 
{
    using emscripten::val;

    if( text.empty() )
    {
        return {};
    }

    auto v = val::global().call< val >( "convert_markdown_to_html"
                                      , text );

    if( !v.as< bool >() )
    {
        return "Error: markdown to html conversion failed";
    }
    else
    {
        return v.as< std::string >();
    }
}

} // namespace kmap