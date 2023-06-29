/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "text_area.hpp"

#include "com/canvas/canvas.hpp"
#include "com/cli/cli.hpp"
#include "com/database/db.hpp"
#include "com/database/root_node.hpp"
#include "com/network/network.hpp"
#include "com/visnetwork/visnetwork.hpp"
#include "emcc_bindings.hpp"
#include "error/result.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/value_or.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <emscripten/val.h>

namespace kmap::com {

TextArea::TextArea( Kmap& km
                  , std::set< std::string > const& requisites
                  , std::string const& description )
    : Component( km, requisites, description )
    , oclerk_{ km }
    , eclerk_{ km, { TextArea::id } }
    , cclerk_{ km }
    , pclerk_{ km }
{
    KM_RESULT_PROLOG();

    register_standard_commands();
    register_standard_outlets();
    KTRYE( register_standard_options() );
    KTRYE( register_panes() );
}

auto TextArea::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( oclerk_.install_registered() );
    KTRY( cclerk_.install_registered() );
    KTRY( eclerk_.install_registered() );
    KTRY( pclerk_.install_registered() );

    KTRY( install_event_sources() );

    KTRY( apply_static_options() );

    rv = outcome::success();

    return rv;
}

auto TextArea::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( oclerk_.check_registered() );
    KTRY( cclerk_.check_registered() );
    KTRY( eclerk_.check_registered() );
    KTRY( pclerk_.check_registered() );

    KTRY( install_event_sources() );
    KTRY( apply_static_options() );

    rv = outcome::success();

    return rv;
}

auto TextArea::apply_static_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const ostore = KTRY( fetch_component< com::OptionStore >() );

    KTRY( ostore->apply( "canvas.editor.background.color" ) );
    KTRY( ostore->apply( "canvas.editor.scrollbar" ) );
    KTRY( ostore->apply( "canvas.editor.text.color" ) );
    KTRY( ostore->apply( "canvas.editor.text.size" ) );
    KTRY( ostore->apply( "canvas.editor.whitespace_wrap" ) );
    KTRY( ostore->apply( "canvas.preview.background.color" ) );
    KTRY( ostore->apply( "canvas.preview.scrollbar" ) );
    KTRY( ostore->apply( "canvas.preview.text.color" ) );
    KTRY( ostore->apply( "canvas.preview.text.size" ) );
    KTRY( ostore->apply( "canvas.preview.whitespace_wrap" ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::register_standard_commands()
    -> void
{
    KM_RESULT_PROLOG();

    {
        auto const action_code =
        R"%%%(
        const selected = kmap.selected_node();
        const body_text_res = kmap.fetch_body( selected );
        let body_text = "";

        // TODO: What if it failed for another reason other than uncreated? Should check failure kind if `has_error()`...
        if( body_text_res.has_value() )
        {
            body_text = body_text_res.value();
        }

        const canvas = kmap.canvas();
        const workspace_pane = canvas.workspace_pane();
        const editor_pane = canvas.editor_pane();
        const editor_pane_str = kmap.uuid_to_string( canvas.editor_pane() );
        const preview_pane = canvas.preview_pane();
        const ta_pane = canvas.text_area_pane();
        const ep_elem = document.getElementById( editor_pane_str );

        ep_elem.value = body_text;

        const old_ws_orient = ktry( canvas.fetch_orientation( workspace_pane ) );
        const old_ta_base = ktry( canvas.fetch_base( ta_pane ) );

        ktry( canvas.orient( workspace_pane, kmap.Orientation.vertical ) );
        ktry( canvas.rebase( ta_pane, 0.33 ) );
        ktry( canvas.rebase( preview_pane, 0.50 ) );
        ktry( canvas.reveal( ta_pane ) );
        ktry( canvas.reveal( editor_pane ) );
        ktry( canvas.reveal( preview_pane ) );
        {
            const opt_root = ktry( kmap.fetch_node( '/meta.setting.option' ) );
            const optn = ktry( kmap.fetch_descendant( opt_root, 'network.viewport_scale' ) );
            ktry( kmap.option_store().apply( optn ) );
        }
        ktry( canvas.redraw() );
        ktry( canvas.focus( editor_pane ) );

        // Remove existing listeners before adding new.
        {
            const listeners = ep_elem.getEventListeners( 'focusout' );
            if( listeners )
            {
                listeners.forEach( function( listener ){ ep_elem.removeEventListener( 'focusout', listener.listener ); } );
            }
        }

        ep_elem.oninput = function( e )
        {
            const editor_panen = kmap.uuid_to_string( kmap.canvas().editor_pane() );
            const md_text = document.getElementById( editor_panen );
            write_preview( convert_markdown_to_html( md_text.value ) );
        };
        ep_elem.addEventListener( 'focusout', function()
        {
            ktry( canvas.orient( workspace_pane, old_ws_orient ) );
            console.log( 'old_ta_base: ' + old_ta_base );
            ktry( canvas.rebase( ta_pane, old_ta_base ) );
            kmap.on_leaving_editor();
            ktry( canvas.redraw() );
            {
                const opt_root = ktry( kmap.fetch_node( '/meta.setting.option' ) );
                const optn = ktry( kmap.fetch_descendant( opt_root, 'network.viewport_scale' ) );
                ktry( kmap.option_store().apply( optn ) );
            }
        } );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "opens body editor pane";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "edit.body"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRYE( cclerk_.register_command( command ) );
    }
}

SCENARIO( "edit.body", "[cmd][text_area][edit.body]" ) // TODO: Move to text_area.cmd component?
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "text_area", "cli", "visnetwork" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );
    auto const vnw = REQUIRE_TRY( km.fetch_component< com::VisualNetwork >() );

    GIVEN( "root node selected" )
    {
        REQUIRE( nw->root_node() == nw->selected_node() );
        // REQUIRE( nw->selected_node() == vnw->selected_node() );

        WHEN( "edit.body executed" )
        {
            REQUIRE_RES( cli->parse_raw( ":edit.body" ) );
        }
    }
}

auto TextArea::register_panes()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( pclerk_.register_pane( Pane{ .id = text_area_uuid
                                     , .heading = "text_area"
                                     , .division = Division{ Orientation::horizontal, 0.660f, false, "div" } } ) );
    KTRY( pclerk_.register_pane( Pane{ .id = editor_uuid
                                     , .heading = "editor"
                                     , .division = Division{ Orientation::horizontal, 0.000f, true, "textarea" } } ) );
    KTRY( pclerk_.register_pane( Pane{ .id = preview_uuid
                                     , .heading = "preview"
                                     , .division = Division{ Orientation::horizontal, 0.000f, false, "div" } } ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::register_standard_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // Preview
    KTRY( oclerk_.register_option( Option{ "canvas.preview.background.color"
                                        , "Sets the background color for the preview pane."
                                        , "\"#222222\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).style.backgroundColor = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.preview.text.color"
                                        , "Sets the text color for the preview pane."
                                        , "\"white\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).style.color = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.preview.text.size"
                                        , "Text size."
                                        , "\"x-large\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).style.fontSize = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.preview.scrollbar"
                                        , "Specify scroll bar."
                                        , "\"auto\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).style.overflow = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.preview.whitespace_wrap"
                                        , "Specify scroll behavior."
                                        , "\"normal\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).style.whiteSpace = option_value;" } ) );
    // Editor
    KTRY( oclerk_.register_option( Option{ "canvas.editor.background.color"
                                        , "Sets the background color for the editor pane."
                                        , "\"#222222\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).style.backgroundColor = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.editor.text.color"
                                        , "Sets the text color for the editor pane."
                                        , "\"white\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).style.color = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.editor.text.size"
                                        , "Text size."
                                        , "\"x-large\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).style.fontSize = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.editor.scrollbar"
                                        , "Specify scroll behavior."
                                        , "\"auto\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).style.overflow = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ "canvas.editor.whitespace_wrap"
                                        , "Specify scroll behavior."
                                        , "\"normal\""
                                        , "document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).style.whiteSpace = option_value;" } ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::register_standard_outlets()
    -> void
{
    eclerk_.register_outlet( Leaf{ .heading = "text_area.load_preview_on_select_node"
                                 , .requisites = { "subject.network", "verb.selected", "object.node" }
                                 , .description = "Loads select node body in preview pane."
                                 , .action = R"%%%(kmap.text_area().load_preview( kmap.selected_node() );)%%%" } );
}

auto TextArea::install_event_sources()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // editor.onkeydown
    {
        // TODO: Needs lots of work, really.
        //       Event should trigger event system fire, not direct call to canvas.
        //       It's mentioned that "on_leaving_editor" is provided elsewhere. Well, it shouldn't be. It should be here, under a separate source that also fires an event.
        auto const ctor =
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).onkeydown = function( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_ctrl = !!e.ctrlKey;
    let is_shift = !!e.shiftKey;

    if(   key == 27/*esc*/
     || ( key == 67/*c*/ && is_ctrl ) )
    {
        console.warn( "TODO: Send leave editor event that is handled by an outlet rather than calling network focus explicitly here in outlet.source." );
        // Focus on network. There's already a callback to call 'on_leaving_editor' on focusout event.
        kmap.canvas().focus( kmap.canvas().network_pane() );
        e.preventDefault();
    }
};)%%%";
        auto const dtor = 
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).onkeydown = null;)%%%";

        scoped_events_.emplace_back( ctor, dtor );
    }

    rv = outcome::success();

    return rv;
}

auto TextArea::load_preview( Uuid const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", id );

    auto rv = result::make_result< void >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // TODO
        })
    ;

    auto const nw = KTRY( fetch_component< com::Network > () );
    auto const db = KTRY( fetch_component< com::Database >() );

    KMAP_ENSURE( nw->exists( id ), error_code::network::invalid_node );

    auto const body = db->fetch_body( nw->resolve( id ) ) | act::value_or( std::string{} ); // TODO: Why not use Kmap::fetch_body? Advantage?

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
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    KTRY( canvas->hide( canvas->editor_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::show_editor()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    KTRY( canvas->reveal( canvas->editor_pane() ) );

    rv = outcome::success();

    return rv;
}

auto TextArea::rebase_pane( float const base )
    -> void
{
    KM_RESULT_PROLOG();

    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    canvas->rebase( canvas->text_area_pane(), base ).value();
}

auto TextArea::rebase_editor_pane( float const base )
    -> void
{
    KM_RESULT_PROLOG();

    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );
    auto const pane = canvas->editor_pane();

    canvas->rebase( pane, base ).value();
}

auto TextArea::rebase_preview_pane( float const base )
    -> void
{
    KM_RESULT_PROLOG();

    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );
    auto const pane = canvas->preview_pane();

    canvas->rebase( pane, base ).value();
}

auto TextArea::show_preview( std::string const& text )
    -> Result< void >
{
    using emscripten::val;

    KM_RESULT_PROLOG();

    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    auto rv = result::make_result< void >();

    val::global().call< val >( "write_preview", text );
    // js::eval_void( io::format( "document.getElementById( '{}' ).innerHTML = '{}';"
    //                          , to_string( canvas.preview_pane() )
    //                          , text ) );

    if( KTRY( canvas->pane_hidden( canvas->preview_pane() ) ) )
    {
        KTRY( canvas->reveal( canvas->preview_pane() ) );
    }

    rv = outcome::success();

    return rv;
}

auto TextArea::hide_preview()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
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
    KM_RESULT_PROLOG();

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
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

       tv->focus_editor();
    }

    auto load_preview( Uuid const& node )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRY( km.fetch_component< com::TextArea >() );

        return tv->load_preview( node );
    }

    auto rebase_pane( float percent )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->rebase_pane( percent );
    }

    auto rebase_preview_pane( float percent )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->rebase_preview_pane( percent );
    }
    
    auto set_editor_text( std::string const& text )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->set_editor_text( text );
    }

    auto show_editor()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRY( km.fetch_component< com::TextArea >() );

        return tv->show_editor();
    }

    auto show_preview( std::string const& body_text )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRY( km.fetch_component< com::TextArea >() );

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
,   std::set({ "canvas.workspace"s, "command.store"s, "command.standard_items"s, "event_store"s, "visnetwork"s }) // TODO: rather than depend on visnetwork, fire events that visnetwork listens for, if initialized.
,   "text_area related functionality"
);

} // namespace text_area_def 
} // namespace anon

} // namespace kmap::com
