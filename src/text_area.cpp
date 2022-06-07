/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "text_area.hpp"

#include "canvas.hpp"
#include "error/result.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"

#include <emscripten.h>
#include <emscripten/val.h>

namespace kmap
{

TextArea::TextArea( Kmap& kmap )
    : kmap_{ kmap }
{
}

auto TextArea::clear()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "clear_text_area" );
}

auto TextArea::set_editor_text( std::string const& text )
    -> void
{
    using emscripten::val;

    val::global().call< val >( "write_text_area", text );
}

auto TextArea::focus_editor()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "focus_text_area" );

    update_pane();
}

auto TextArea::focus_preview()
    -> void
{
    using emscripten::val;

    val::global().call< val >( "focus_preview" );

    update_pane();
}

auto TextArea::editor_contents()
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

auto TextArea::hide_editor()
    -> Result< void >
{
    auto rv = error::make_result< void >();

    KTRY( kmap_.canvas().hide( kmap_.canvas().editor_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::show_editor()
    -> Result< void >
{
    auto rv = error::make_result< void >();

    KTRY( kmap_.canvas().reveal( kmap_.canvas().editor_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::rebase_pane( float const base )
    -> void
{
    auto& canvas = kmap_.canvas();

    canvas.rebase( canvas.text_area_pane(), base ).value();
}

auto TextArea::rebase_editor_pane( float const base )
    -> void
{
    auto& canvas = kmap_.canvas();
    auto const pane = canvas.editor_pane();

    canvas.rebase( pane, base ).value();
}

auto TextArea::rebase_preview_pane( float const base )
    -> void
{
    auto& canvas = kmap_.canvas();
    auto const pane = canvas.preview_pane();

    canvas.rebase( pane, base ).value();
}

auto TextArea::show_preview( std::string const& text )
    -> Result< void >
{
    using emscripten::val;

    auto rv = error::make_result< void >();
    auto& canvas = kmap_.canvas();

    val::global().call< val >( "write_preview", text );
    // js::eval_void( io::format( "document.getElementById( '{}' ).innerHTML = '{}';"
    //                          , to_string( canvas.preview_pane() )
    //                          , text ) );

    KTRY( canvas.reveal( canvas.preview_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::hide_preview()
    -> Result< void >
{
    auto rv = error::make_result< void >();

    KTRY( kmap_.canvas().hide( kmap_.canvas().preview_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::resize_preview( std::string const& attr )
    -> void
{
    using emscripten::val;

    val::global().call< val >( "resize_preview", attr );

    update_pane();
}

auto TextArea::update_pane()
    -> void
{
    kmap_.canvas().update_pane( kmap_.canvas().text_area_pane() ).value();
}

} // namespace kmap
