/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "text_view.hpp"

#include "io.hpp"

#include <emscripten.h>
#include <emscripten/val.h>

namespace kmap
{

auto TextView::clear()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "clear_text_view" );
}

auto TextView::write( std::string const& text )
    -> void
{
    using emscripten::val;

    val::global().call< val >( "write_text_view"
                             , text );
}

auto TextView::focus_editor()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "focus_text_view" );
}

auto TextView::focus_preview()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "focus_preview" );
}

auto TextView::editor_contents()
    -> std::string
{
    using emscripten::val;

    auto rv = val::global().call< val >( "get_editor_contents" );

    if( !rv.as< bool >() )
    {
        return {};
    }

    return rv.as< std::string >();
}

auto TextView::hide_editor()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "hide_text_view" );
}

auto TextView::show_editor()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "show_text_view" );
}

auto TextView::resize_editor( std::string const& attr )
    -> void
{
    using emscripten::val;

    val::global().call< val >( "resize_text_view"
                             , attr );
}

auto TextView::show_preview( std::string const& text )
    -> void
{
    using emscripten::val;

    val::global().call< val >( "write_preview"
                             , text );
}

auto TextView::hide_preview()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "hide_preview" );
}

auto TextView::resize_preview( std::string const& attr )
    -> void
{
    using emscripten::val;

    val::global().call< val >( "resize_preview"
                             , attr );
}

} // namespace kmap
