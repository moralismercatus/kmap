/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/text_area/text_area.hpp>

#include <com/canvas/canvas.hpp>
#include <com/cli/cli.hpp>
#include <com/database/db.hpp>
#include <com/database/root_node.hpp>
#include <com/network/network.hpp>
#include <com/visnetwork/visnetwork.hpp>
#include <error/result.hpp>
#include <io.hpp>
#include <kmap.hpp>
#include <path/act/value_or.hpp>
#include <test/util.hpp>
#include <util/markdown.hpp>
#include <util/result.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <catch2/catch_test_macros.hpp>

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

KMAP_LOG_LINE();
    KTRY( oclerk_.check_registered() );
KMAP_LOG_LINE();
    KTRY( cclerk_.check_registered() );
KMAP_LOG_LINE();
    KTRY( eclerk_.check_registered() );
KMAP_LOG_LINE();
    KTRY( pclerk_.check_registered() );

KMAP_LOG_LINE();
    KTRY( install_event_sources() );
KMAP_LOG_LINE();
    KTRY( apply_static_options() );
KMAP_LOG_LINE();

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
            ktry( kmap.on_leaving_editor() );
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

#if !KMAP_NATIVE
SCENARIO( "edit.body", "[cli][text_area][edit.body]" ) // TODO: Move to text_area.cmd component?
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "text_area", "cli", "visnetwork", "canvas", "network" );
    KMAP_DISABLE_DEBOUNCE_FIXTURE_SCOPED();

    auto& km = Singleton::instance();
    auto const canvas = REQUIRE_TRY( km.fetch_component< com::Canvas >() );
    auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const vnw = REQUIRE_TRY( km.fetch_component< com::VisualNetwork >() );

    GIVEN( "root node selected" )
    {
        REQUIRE( nw->root_node() == nw->selected_node() );
        REQUIRE_TRY( canvas->update_all_panes() );

        THEN( "editor should be hidden" )
        {
            REQUIRE( true == REQUIRE_TRY( js::eval< bool >( "return document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).hidden;" ) ) );
        }

        GIVEN( "edit.body executed" )
        {
            REQUIRE_RES( cli->parse_raw( ":edit.body" ) );

            THEN( "editor should NOT be hidden" )
            {
                REQUIRE( false == REQUIRE_TRY( js::eval< bool >( "return document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).hidden;" ) ) );
            }

            GIVEN( "leave editor via select node" )
            {
                REQUIRE_TRY( nw->select_node( nw->selected_node() ) );

                THEN( "editor should be hidden" )
                {
                    vnw->focus();
                    REQUIRE( true == REQUIRE_TRY( js::eval< bool >( "return document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).hidden;" ) ) );
                }
            }
        }
    }
}
#endif // !KMAP_NATIVE

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
                                 , .action = R"%%%(ktry( kmap.text_area().load_preview( kmap.selected_node() ) );)%%%" } );
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

#if !KMAP_NATIVE
    KTRY( show_preview( markdown_to_html( body ) ) );
#endif //!KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

#if !KMAP_NATIVE
SCENARIO( "TextArea::load_preview", "[text_area][js]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "text_area", "network", "visnetwork" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const ta = REQUIRE_TRY( km.fetch_component< com::TextArea >() );

    GIVEN( "root body => multiline text")
    {
        auto const text = "This is.\n\na multiline `nested backtick` text";

        REQUIRE_TRY( nw->update_body( nw->root_node(), text ) );

        GIVEN( "load_preview( root_node )" )
        {
            REQUIRE_TRY( ta->load_preview( nw->root_node() ) );

            THEN( "pane content matches expected" )
            {
                auto const elem_text = REQUIRE_TRY( js::eval< std::string >( "return document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).innerHTML;" ) );
                
                REQUIRE( elem_text == markdown_to_html( text ) );
            }
        }
    }
}
#endif // !KMAP_NATIVE

auto TextArea::clear()
    -> Result< void >
{
    KM_RESULT_PROLOG();

#if !KMAP_NATIVE
    KTRY( js::eval_void( "clear_text_area();" ) );
#endif // !KMAP_NATIVE

    return outcome::success();
}

auto TextArea::set_editor_text( std::string const& text )
    -> Result< void >
{
    KM_RESULT_PROLOG();

#if !KMAP_NATIVE
    KTRY( js::eval_void( fmt::format( "write_text_area( '{}' );", text ) ) );
#endif // !KMAP_NATIVE

    return outcome::success();
}

auto TextArea::focus_editor()
    -> Result< void >
{
    KM_RESULT_PROLOG();

#if !KMAP_NATIVE
    KTRY( js::eval_void( "focus_text_area();" ) );
#endif // !KMAP_NATIVE
    KTRY( update_pane() );

    return outcome::success();
}

auto TextArea::focus_preview()
    -> Result< void >
{
    KM_RESULT_PROLOG();

#if !KMAP_NATIVE
    KTRY( js::eval_void( "focus_preview();" ) );
#endif // !KMAP_NATIVE
    KTRY( update_pane() );

    return outcome::success();
}

auto TextArea::editor_contents()
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();

#if !KMAP_NATIVE
    rv = KTRY( js::eval< std::string >( "return document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) ).value;" ) );
#endif // !KMAP_NATIVE

    return rv;
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
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    KTRY( canvas->rebase( canvas->text_area_pane(), base ) );
    
    return outcome::success();
}

auto TextArea::rebase_editor_pane( float const base )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto const canvas = KTRYE( fetch_component< com::Canvas >() );
    auto const pane = canvas->editor_pane();

    KTRY( canvas->rebase( pane, base ) );

    return outcome::success();
}

auto TextArea::rebase_preview_pane( float const base )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto const canvas = KTRYE( fetch_component< com::Canvas >() );
    auto const pane = canvas->preview_pane();

    KTRY( canvas->rebase( pane, base ) );

    return outcome::success();
}

// TODO:
//     Q: What does the relationship between TextArea and JS look like? Are they inseparable? No JS, no TA?
//     A: Maybe... right now, very much intertwined. TA depends on div, and largely looks like a C++ interface to it.
//        On the other hand, it has distinct kmap aspects, such as events, events, commands, options, etc.
auto TextArea::show_preview( std::string const& text )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "text", text );

    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    auto rv = result::make_result< void >();
    auto const escaped_text = replace_unescaped_char( text, '`', R"(\\`)" );

#if !KMAP_NATIVE
    KTRY( js::eval_void( fmt::format( "document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).innerHTML = `{}`;", escaped_text ) ) );

    if( KTRY( canvas->pane_hidden( canvas->preview_pane() ) ) )
    {
        KTRY( canvas->reveal( canvas->preview_pane() ) );
    }
#endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

#if !KMAP_NATIVE
SCENARIO( "TextArea::show_preview", "[text_area][js]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "text_area", "network", "visnetwork" );

    auto& km = Singleton::instance();
    auto const ta = REQUIRE_TRY( km.fetch_component< com::TextArea >() );

    REQUIRE_TRY( ta->show_preview( "hi der" ) );
    REQUIRE_TRY( ta->show_preview( "hi `der`" ) ); // Backticks don't break system.
}
#endif // !KMAP_NATIVE

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
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "attr", attr );

#if !KMAP_NATIVE
    KTRY( js::eval_void( fmt::format( "resize_preview( '{}' );", attr ) ) );
#endif // !KMAP_NATIVE

    KTRY( update_pane() );

    return outcome::success();
}

auto TextArea::update_pane()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto const canvas = KTRY( fetch_component< com::Canvas >() );

    KTRY( canvas->update_pane( canvas->text_area_pane() ) );

    return outcome::success();
}

namespace {
namespace text_area_def {

using namespace std::string_literals;

#if !KMAP_NATIVE
REGISTER_COMPONENT
(
    kmap::com::TextArea
,   std::set({ "canvas.workspace"s, "command.store"s, "command.standard_items"s, "event_store"s, "visnetwork"s }) // TODO: rather than depend on visnetwork, fire events that visnetwork listens for, if initialized.
,   "text_area related functionality"
);
#else
REGISTER_COMPONENT
(
    kmap::com::TextArea
,   std::set({ "canvas.workspace"s, "command.store"s, "command.standard_items"s, "event_store"s })
,   "text_area related functionality"
);
#endif // !KMAP_NATIVE

} // namespace text_area_def 
} // namespace anon

} // namespace kmap::com
