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
const js_beautify = require( 'js-beautify' ).js;
require( 'geteventlisteners' ); // This works by overriding Element.prototype.addEventListener and Element.prototype.removeEventListener upon import, so only after here are they overriden.

kmap_pretest_options = '--durations=yes'
kmap_pretest_options += ' --verbosity=high'
// kmap_pretest_options += ' --skip-benchmarks'
kmap_pretest_options += ' --benchmark-samples=1'
// kmap_pretest_options += ' --success'
let kmap_pretest_targets = ''
kmap_pretest_targets = '*' // All
// kmap_pretest_targets += ',Scenario: AliasSet basic ops'
// kmap_pretest_targets += ',Scenario: AliasStore::fetch_aliases'
// kmap_pretest_targets += ',Scenario: AliasStore::fetch_dsts'
// kmap_pretest_targets += ',Scenario: AliasStore::fetch_alias_children'
// kmap_pretest_targets += ',Scenario: Child::compare'
// kmap_pretest_targets += ',Scenario: Database::push_child'
// kmap_pretest_targets += ',Scenario: EventStore::fire_event'
// kmap_pretest_targets += ',Scenario: Network::create_alias'
// kmap_pretest_targets += ',Scenario: Network::create_child'
// kmap_pretest_targets += ',Scenario: Network::erase_node'
// kmap_pretest_targets += ',Scenario: Network::fetch_parent'
// kmap_pretest_targets += ',Scenario: Network::is_alias'
// kmap_pretest_targets += ',Scenario: Network::resolve'
// kmap_pretest_targets += ',Scenario: TaskStore::create_subtask'
// kmap_pretest_targets += ',Scenario: edit.body'
// kmap_pretest_targets += ',Scenario: view::Alias::fetch'
// kmap_pretest_targets += ',Scenario: view::AllOf::fetch'
// kmap_pretest_targets += ',Scenario: view::Ancestor::fetch'
// kmap_pretest_targets += ',Scenario: view::Link::operator<'
// kmap_pretest_targets += ',Scenario: view::event::Requisite::fetch'
// kmap_pretest_targets += ',Scenario: view::operator|Link Link'
// kmap_pretest_targets += ',Scenario: view::operator|TetherCT Link'
// kmap_pretest_targets += ',[abs_path],[abs_path_flat]'
// kmap_pretest_targets += ',[alias]'
// kmap_pretest_targets += ',[autosave]'
// kmap_pretest_targets += ',[benchmark]'
// kmap_pretest_targets += ',[cmd]'
// kmap_pretest_targets += ',[cmd][log][task]'
// kmap_pretest_targets += ',[cmd][task]'
// kmap_pretest_targets += ',[com][nw][alias]'
// kmap_pretest_targets += ',[com][text_area]'
// kmap_pretest_targets += ',[db][fs]'
// kmap_pretest_targets += ',[event]' 
kmap_pretest_targets += ',[iface]'
kmap_pretest_targets += ',[link]'
kmap_pretest_targets += ',[kmap_iface]'
// kmap_pretest_targets += ',[move_node]'
// kmap_pretest_targets += ',[node_view]'
// kmap_pretest_targets += ',[nw]'
// kmap_pretest_targets += ',[parser]'
// kmap_pretest_targets += ',[path][node_view]'
// kmap_pretest_targets += ',[profile]'
// kmap_pretest_targets += ',[tag]'
// kmap_pretest_targets += ',[take]'
// kmap_pretest_targets += ',[task]'
// kmap_pretest_targets += ',[text_area]'
// kmap_pretest_targets += ',[query_cache]'

function beautify_javascript( code )
{
    return js_beautify( code );
}

// [WARNING] Not thread safe! Uses global object 'jshint'!
function lint_javascript( code )
{
    try
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
    catch( err )
    {
        console.log( String( err ) );
    }
}

function to_VectorString( li )
{
    try
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
    catch( err )
    {
        console.log( String( err ) );
    }
}

function clear_cli_error()
{
    try
    {
        let cli = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );

        cli.style.backgroundColor = 'white';
        cli.readOnly = false;
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function clear_canvas_cli()
{
    try
    {
        let cli = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );

        cli.value = "";
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

/// Emscripten Represents C++ exceptions by pointer to linear address / number.
/// Only guaranteed to work when 'err_obj' is the object passed to catch().
function is_cpp_exception( err_obj )
{
    return typeof err_obj === 'number';
}

function reset_canvas_cli()
{
    try
    {
        clear_cli_error();
        reset_cli_error();
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function clear_text_area()
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

        tv.value = "";
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function write_text_area( text )
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

        tv.value = text;
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function focus_text_area()
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

        tv.focus();
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function focus_preview()
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value() );

        tv.focus();
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function get_editor_contents()
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

        return tv.value;
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function write_preview( text )
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value() ).innerHTML = text;
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function resize_preview( attr )
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ).value() );

        tv.setAttribute( "style", attr );
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function convert_markdown_to_html( text )
{
    try
    {
        var converter = new showdown.Converter( { extensions: [ showdown_highlight ]
                                                , tables: true
                                                , tasklists: true
                                                , strikethrough: true } );

        return converter.makeHtml( text );
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function append_script( fn, code )
{
    try
    {
        var old_script = document.getElementById( fn );

        if( old_script != null )
        {
            console.assert( document.body.removeChild( old_script ) ); // TODO: Replace with old_script.parentNode.removeChild(). This is safer. Ensures true parent
        }

        var script = document.createElement( "script" );

        script.id = fn;
        script.type = "text/javascript";
        script.textContent = "//<![#PCDATA[\n function " + fn + "() { " + code + " }\n //]]>";

        document.body.appendChild( script );
    }
    catch( err )
    {
        console.log( String( err ) );
    }
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

// window.onkeydown = function( e )
// {
//     try
//     {
//         let key = e.keyCode ? e.keyCode : e.which;
//         let is_shift = !!e.shiftKey;
//         let cli = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );
//         let editor = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() );

//         switch ( key )
//         {
//             case 186: // colon
//             {
//                 if( is_shift
//                 && cli != document.activeElement
//                 && editor != document.activeElement )
//                 {
//                     clear_cli_error();
//                     cli.value = "";
//                     kmap.cli().focus();
//                 }
//                 break;
//             }
//             case 50: // '2' for '@'
//             {
//                 if( is_shift
//                 && cli != document.activeElement
//                 && editor != document.activeElement )
//                 {
//                     clear_cli_error();
//                     cli.value = ":";
//                     kmap.cli().focus();
//                 }
//                 break;
//             }
//             case 191: // forward slash
//             {
//                 if( cli != document.activeElement
//                 && editor != document.activeElement )
//                 {
//                     clear_cli_error();
//                     cli.value = ":";
//                     kmap.cli().focus();
//                 }
//                 break;
//             }
//             case 13: // enter
//             {
//                 break;
//             }
//         }
//     }
//     catch( err )
//     {
//         console.log( String( err ) );
//     }
// }

function debounce( fn, timer_name, timeout )
{
  return function(...args) 
  {
    if( global[ timer_name ] !== undefined )
    {
        clearTimeout( global[ timer_name ] );
    }

    global[ timer_name ] = setTimeout( function(){ fn.apply( this, args ); }, timeout );
  };
}
