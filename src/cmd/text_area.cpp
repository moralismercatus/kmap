/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "text_area.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

namespace kmap::cmd {

namespace { 
namespace edit_body_def {

auto const guard_code =
R"%%%(```javascript
    return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.failure( 'failed to open editor' );
const selected = kmap.selected_node();
const body_text = kmap.fetch_body( selected );

if( body_text.has_value() )
{
    const canvas = kmap.canvas();
    const workspace_pane = canvas.workspace_pane();
    const editor_pane = canvas.editor_pane();
    const editor_pane_str = kmap.uuid_to_string( canvas.editor_pane() ).value();
    const preview_pane = canvas.preview_pane();
    const ta_pane = canvas.text_area_pane();
    const ep_elem = document.getElementById( editor_pane_str );

    ep_elem.value = body_text.value();

    const old_ws_orient = canvas.fetch_orientation( workspace_pane ).value_or_throw();
    const old_ta_base = canvas.fetch_base( ta_pane ).value_or_throw();

    canvas.orient( workspace_pane, kmap.Orientation.vertical );
    canvas.rebase( ta_pane, 0.33 ).throw_on_error();
    canvas.rebase( preview_pane, 0.50 ).throw_on_error();
    canvas.reveal( editor_pane ).throw_on_error();
    canvas.reveal( preview_pane ).throw_on_error();
    canvas.focus( editor_pane );

    ep_elem.oninput = function( e )
    {
        const md_text = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );
        write_preview( convert_markdown_to_html( md_text.value ) );
    };
    ep_elem.addEventListener( 'focusout', function()
    {
        canvas.orient( workspace_pane, old_ws_orient ).throw_on_error();
        canvas.rebase( ta_pane, old_ta_base ).throw_on_error();
        kmap.on_leaving_editor();
    } );

    rv = kmap.success( 'editor opened' );

    return rv;
}
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "Opens body editor for selected node";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    edit.body
,   description 
,   arguments
,   guard
,   action
);

namespace edit_def {
    REGISTER_COMMAND // TODO: Use REGISTER_COMMAND_ALIAS instead.
    (
        edit
    ,   description 
    ,   arguments
    ,   guard
    ,   action
    );
} // namespace edit_def 
} // namespace edit_body_def 
} // namespace anon

} // namespace kmap::cmd