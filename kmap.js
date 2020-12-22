/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

const showdown = require( 'showdown' )
               , showdown_highlight = require( 'showdown-highlight' )
               ;

function clear_cli_error()
{
    let cli = document.getElementById( 'cli_input' );

    cli.style = 'background-color: white';
    cli.readOnly = false;
}

function clear_cli_input()
{
    let cli = document.getElementById( 'cli_input' );

    cli.value = "";
}

function reset_cli_input()
{
    clear_cli_error();
    reset_cli_error();
}

function clear_text_view()
{
    let tv = document.getElementById( "text_area" );

    tv.value = "";
}

function write_text_view( text )
{
    let tv = document.getElementById( "text_area" );

    tv.value = text;
}

function focus_text_view()
{
    let tv = document.getElementById( "text_area" );

    tv.focus();
}

function focus_preview()
{
    let tv = document.getElementById( "div_preview" );

    tv.focus();
}

function hide_text_view()
{
    let tv = document.getElementById( "text_area" );

    tv.hidden = true;
}

function show_text_view()
{
    let tv = document.getElementById( "text_area" );

    tv.hidden = false;
}

function resize_text_view( attr )
{
    let tv = document.getElementById( "text_area" );

    tv.setAttribute( "style"
                   , attr );
}

function get_editor_contents()
{
    let tv = document.getElementById( "text_area" );

    return tv.value;
}

function write_preview( text )
{
    document.getElementById( "div_preview" ).innerHTML = text;
}

function show_preview()
{
    let tv = document.getElementById( "text_area" );

    tv.hidden = false;
}

function hide_preview()
{
    let tv = document.getElementById( "div_preview" );

    tv.hidden = true;
}

function resize_preview( attr )
{
    let tv = document.getElementById( "div_preview" );

    tv.setAttribute( "style"
                   , attr );
}

function convert_markdown_to_html( text )
{
    var converter = new showdown.Converter( { extensions: [ showdown_highlight ]
                                            , tables: true
                                            , tasklists: true
                                            , strikethrough: true } );

    return converter.makeHtml( text );
}

function append_script( fn, code )
{
    var old_script = document.getElementById( fn );

    if( old_script != null )
    {
        document.body.removeChild( old_script );
    }

    var script = document.createElement( "script" );

    script.id = fn;
    script.type = "text/javascript";
    script.textContent = "//<![#PCDATA[\n function " + fn + "() { " + code + " }\n //]]>";

    document.body.appendChild( script );
}

function eval_js_command( code )
{
    try
    {
        const res = eval( code );

        console.log( 'res: ' + res );

        if( res[ 0 ] )
        {
            kmap.cli().notify_success( res[ 1 ] );
        }
        else
        {
            kmap.cli().notify_failure( res[ 1 ] );
        }
    }
    catch( err )
    {
        // TODO: Should return error info message, to help user debug...
        console.error( "code execution failed: " + err.message );

        return false;
    }

    return true;
}

function eval_code( code )
{
    try
    {
        eval( code );
    }
    catch( err )
    {
        // TODO: Should return error info message, to help user debug...
        console.error( "code execution failed: " + err.message );
        return false;
    }

    return true;
}

document.getElementById( "text_area" ).oninput = function( e )
{
    var md_text = document.getElementById( "text_area" );

    write_preview( convert_markdown_to_html( md_text.value ) );
}

document.getElementById( "text_area" ).addEventListener( 'focusout', function()
{
    Module.on_leaving_editor();
} );

window.onkeydown = function( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_shift = !!e.shiftKey;
    let cli = document.getElementById( "cli_input" );
    let editor = document.getElementById( "text_area" );

    switch ( key )
    {
        case 186: // colon
        {
            if( is_shift
             && cli != document.activeElement
             && editor != document.activeElement )
            {
                clear_cli_error();
                cli.value = "";
                cli.focus();
            }
            break;
        }
        case 50: // '2' for '@'
        {
            if( is_shift
             && cli != document.activeElement
             && editor != document.activeElement )
            {
                clear_cli_error();
                cli.value = ":";
                cli.focus();
            }
            break;
        }
        case 191: // forward slash
        {
             if( cli != document.activeElement
              && editor != document.activeElement )
            {
                clear_cli_error();
                cli.value = ":";
                cli.focus();
            }
            break;
        }
        case 13: // enter
        {
            break;
        }
    }
}

document.getElementById( "cli_input" ).onkeyup = function( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let text = document.getElementById( "cli_input" );
    let is_ctrl = !!e.ctrlKey;
    let is_shift = !!e.shiftKey;

    Module.cli_on_key_up( key
                        , is_ctrl
                        , is_shift );
}

document.getElementById( "cli_input" ).onkeydown = function( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let text = document.getElementById( "cli_input" );

    if( 9 == key ) // tab
    {
        Module.complete_cli( text.value );
        e.preventDefault();
    }
}

document.getElementById( "text_area" ).onkeydown = function( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_ctrl = !!e.ctrlKey;

    if( 27 == key // escape
     || ( is_ctrl && 67 == key ) ) // ctrl+c
    {
        Module.focus_network();
    }
}

