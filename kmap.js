/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

kmap = Module
const showdown = require( 'showdown' )
               , showdown_highlight = require( 'showdown-highlight' )
               ;
const jshint = require( 'jshint' ).JSHINT;

// [WARNING] Not thread safe! Uses global object 'jshint'!
function lint_javascript( code )
{
    let rv = null;
    const options = { 'esversion': 6 // Just guessing on the EMCAScript version. May need to bump in the future.
                    , 'laxcomma': true  // Allows for `, line_item` style of line breaking.
                    , 'laxbreak': true }; // Allows for `|| line_item` style of line breaking.

    if( !jshint( code, options ) )
    {
        rv = '';

        for( let i = 0
           ; i < jshint.errors.length
           ; ++i )
        {
            error = jshint.errors[ i ];

            rv += [ 'error #: ' + i
                  , 'scope: ' + error.scope
                  , 'reason: ' + error.reason
                  , 'evidence: ' + error.evidence
                  , 'raw: ' + error.raw 
                  , 'id: ' + error.id 
                  , 'line: ' + error.line
                  , 'character: ' + error.character
                  , 'code: ' + error.code
                  ].join( '\n' ) + '\n';
        }

        rv += 'full source: ' + code;
    }

    return rv;
}

function to_VectorString( li )
{
    let v = new kmap.VectorString();

    for( i = 0
       ; i < li.length
       ; ++i )
    {
        v.push_back( li[ i ] );
    }

    return v;
}

function clear_cli_error()
{
    let cli = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );

    cli.style = 'background-color: white';
    cli.readOnly = false;
}

function clear_canvas_cli()
{
    let cli = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );

    cli.value = "";
}

function reset_canvas_cli()
{
    clear_cli_error();
    reset_cli_error();
}

function clear_text_area()
{
    let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

    tv.value = "";
}

function write_text_area( text )
{
    let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

    tv.value = text;
}

function focus_text_area()
{
    let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

    tv.focus();
}

function focus_preview()
{
    let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value() );

    tv.focus();
}

function get_editor_contents()
{
    let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

    return tv.value;
}

function write_preview( text )
{
    let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value() ).innerHTML = text;
}

function resize_preview( attr )
{
    let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value() );

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

window.onkeydown = function( e )
{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_shift = !!e.shiftKey;
    let cli = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );
    let editor = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

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
                kmap.cli().focus();
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
                kmap.cli().focus();
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
                kmap.cli().focus();
            }
            break;
        }
        case 13: // enter
        {
            break;
        }
    }
}
