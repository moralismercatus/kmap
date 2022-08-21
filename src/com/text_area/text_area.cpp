/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "text_area.hpp"

#include "com/canvas/canvas.hpp"
#include "com/database/db.hpp"
#include "com/network/network.hpp"
#include "emcc_bindings.hpp"
#include "error/result.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/value_or.hpp"

#include <emscripten.h>
#include <emscripten/val.h>

namespace kmap::com {

TextArea::TextArea( Kmap& kmap
                  , std::set< std::string > const& requisites
                  , std::string const& description )
    : Component( kmap, requisites, description )
    , eclerk_{ kmap }
{
}

auto TextArea::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( install_event_outlets() );
    KTRY( install_event_sources() );

    rv = outcome::success();

    return rv;
}

auto TextArea::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto TextArea::install_event_outlets()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.install_outlet( Leaf{ .heading = "text_area.load_preview_on_select_node"
                                      , .requisites = { "subject.kmap", "verb.selected", "object.node" }
                                      , .description = "loads select node body in preview pane"
                                      , .action = R"%%%(kmap.text_area().load_preview( kmap.selected_node() );)%%%" } ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::install_event_sources()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // editor.onkeydown
    {
        // TODO: Needs lots of work, really.
        //       Event should trigger event system fire, not direct call to canvas.
        //       It's mentioned that "on_leaving_editor" is provided elsewhere. Well, it shouldn't be. It should be here, under a separate source that also fires an event.
        auto const ctor =
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() ).onkeydown = function( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_ctrl = !!e.ctrlKey;
    let is_shift = !!e.shiftKey;

    if(   key == 27/*esc*/
     || ( key == 67/*c*/ && is_ctrl ) )
    {
        // Focus on network. There's already a callback to call 'on_leaving_editor' on focusout event.
        kmap.canvas().focus( kmap.canvas().network_pane() );
        e.preventDefault();
    }
};)%%%";
        auto const dtor = 
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() ).onkeydown = null;)%%%";

        scoped_events_.emplace_back( ctor, dtor );
    }

    rv = outcome::success();

    return rv;
}

auto TextArea::load_preview( Uuid const& id )
    -> Result< void >
{
    auto rv = error::make_result< void >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // TODO
        })
    ;

    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network > () );
    auto const db = KTRY( fetch_component< com::Database >() );

    KMAP_ENSURE( nw->exists( id ), error_code::network::invalid_node );

    auto const body = db->fetch_body( nw->alias_store().resolve( id ) ) | act::value_or( std::string{} ); // TODO: Why not use Kmap::fetch_body? Advantage?

    KTRY( show_preview( markdown_to_html( body ) ) );

    rv = outcome::success();

    return rv;
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
    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    KTRY( canvas->hide( canvas->editor_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::show_editor()
    -> Result< void >
{
    auto rv = error::make_result< void >();
    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    KTRY( canvas->reveal( canvas->editor_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::rebase_pane( float const base )
    -> void
{
    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    canvas->rebase( canvas->text_area_pane(), base ).value();
}

auto TextArea::rebase_editor_pane( float const base )
    -> void
{
    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );
    auto const pane = canvas->editor_pane();

    canvas->rebase( pane, base ).value();
}

auto TextArea::rebase_preview_pane( float const base )
    -> void
{
    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );
    auto const pane = canvas->preview_pane();

    canvas->rebase( pane, base ).value();
}

auto TextArea::show_preview( std::string const& text )
    -> Result< void >
{
    using emscripten::val;

    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    auto rv = error::make_result< void >();

    val::global().call< val >( "write_preview", text );
    // js::eval_void( io::format( "document.getElementById( '{}' ).innerHTML = '{}';"
    //                          , to_string( canvas.preview_pane() )
    //                          , text ) );

    KTRY( canvas->reveal( canvas->preview_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::hide_preview()
    -> Result< void >
{
    auto rv = error::make_result< void >();
    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    KTRY( canvas->hide( canvas->preview_pane() ) );

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
    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    canvas->update_pane( canvas->text_area_pane() ).value();
}

namespace binding {

using namespace emscripten;

struct TextArea
{
    Kmap& km;

    TextArea( Kmap& kmap )
        : km{ kmap }
    {
    }

    auto focus_editor()
        -> void
    {
        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

       tv->focus_editor();
    }

    auto load_preview( Uuid const& node )
        -> kmap::binding::Result< void >
    {
        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        return tv->load_preview( node );
    }

    auto rebase_pane( float percent )
        -> void
    {
        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->rebase_pane( percent );
    }

    auto rebase_preview_pane( float percent )
        -> void
    {
        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->rebase_preview_pane( percent );
    }
    
    auto set_editor_text( std::string const& text )
        -> void
    {
        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->set_editor_text( text );
    }

    auto show_editor()
        -> kmap::binding::Result< void >
    {
        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        return tv->show_editor();
    }

    auto show_preview( std::string const& body_text )
        -> kmap::binding::Result< void >
    {
        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        return tv->show_preview( body_text );
    }
};

auto text_area()
    -> binding::TextArea
{
    auto& kmap = Singleton::instance();

    return binding::TextArea{ kmap };
}

EMSCRIPTEN_BINDINGS( kmap_text_area )
{
    function( "text_area", &kmap::com::binding::text_area );
    class_< kmap::com::binding::TextArea >( "TextArea" )
        .function( "focus_editor", &kmap::com::binding::TextArea::focus_editor )
        .function( "load_preview", &kmap::com::binding::TextArea::load_preview )
        .function( "rebase_pane", &kmap::com::binding::TextArea::rebase_pane )
        .function( "rebase_preview_pane", &kmap::com::binding::TextArea::rebase_preview_pane )
        .function( "set_editor_text", &kmap::com::binding::TextArea::set_editor_text )
        .function( "show_editor", &kmap::com::binding::TextArea::show_editor )
        .function( "show_preview", &kmap::com::binding::TextArea::show_preview )
        ;
}

} // namespace binding

namespace {
namespace text_area_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::TextArea
,   std::set({ "canvas"s, "event_store"s })
,   "text_area related functionality"
);

} // namespace text_area_def 
}

} // namespace kmap::com
