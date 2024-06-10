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
const jscodeshift = require('jscodeshift');
const vexjs = require( 'vex-js' )
require( 'geteventlisteners' ); // This works by overriding Element.prototype.addEventListener and Element.prototype.removeEventListener upon import, so only after here are they overriden.

vexjs.registerPlugin( require( 'vex-dialog' ) )
vexjs.defaultOptions.className = 'vex-theme-os'

kmap.flags = { 'debounce': true };

// Note: Unfortunately, these cannot be simply accounted for by replacing all instaces of `ktry( <expr> )` with an IIFE (immediately invoked function expr).
//       The point is to return to the nearest exit point - which would be the IIFE's exit point - not the intended function exit point! It would be useless.
// TODO: These should ensure that ktry() has a single argument. If not, they shouldn't match. So, `ktry()` and `ktry( <expr>, ... )` should fail.
function kmap_preprocess_js_script( code )
{
    const cshift = jscodeshift;
    const collection = jscodeshift( code );
    // <qualifier> <var> = ktry( <expr> )
    collection.find( jscodeshift.VariableDeclaration, { declarations: [ { type: 'VariableDeclarator'
                                                                        , init: { type: 'CallExpression'
                                                                                , callee: { type: 'Identifier'
                                                                                          , name: 'ktry' } } } ] } )
        .replaceWith( ( decl_node_path ) => {
            ktry_tmp = decl_node_path.node;
            const decl = decl_node_path.node.declarations[ 0 ];
            const decl_name = decl.id.name;
            const tmp_var_name = 'ktry_postproc_temp_val_' + decl_name;
            const ktry_call = decl.init;
            const ktry_arg = ktry_call.arguments[ 0 ];
            const decl_kind =  decl_node_path.node.kind; 

            const tmp_decl = cshift.variableDeclaration( decl_kind, [ cshift.variableDeclarator( cshift.identifier( tmp_var_name ), ktry_arg ) ] );
            const condition = cshift.callExpression( cshift.memberExpression( cshift.identifier( tmp_var_name ), cshift.identifier( 'has_error' ) ), [] );
            const ret_result = cshift.returnStatement( cshift.callExpression( cshift.memberExpression( cshift.identifier( tmp_var_name ), cshift.identifier( 'error' ) ), [] ) );
            const if_stmt = cshift.ifStatement( condition, cshift.blockStatement( [ ret_result ] ) );
            const tmp_value_call = cshift.callExpression( cshift.memberExpression( cshift.identifier( tmp_var_name ), cshift.identifier( 'value' ) ), [] );
            const new_decl = cshift.variableDeclaration( decl_kind, [ cshift.variableDeclarator( cshift.identifier( decl_name ), tmp_value_call ) ] );

            return [ tmp_decl, if_stmt, new_decl ];
        } ); 
    // <var> = ktry( <expr> );
    collection.find( cshift.AssignmentExpression, { left: { type: 'Identifier' }
                                                  , right: { type: 'CallExpression'
                                                           , callee: { type: 'Identifier'
                                                                     , name: 'ktry' } } } )
        .replaceWith( ( assign_node_path ) => {
            ktry_tmp = assign_node_path.node;
            const assignment = assign_node_path.node;
            const lhs = assignment.left;
            const lhs_name = lhs.name;
            const rhs = assignment.right;
            const tmp_var_name = 'ktry_postproc_temp_val_' + lhs_name;
            const ktry_call = rhs;
            const ktry_arg = ktry_call.arguments[ 0 ];

            const tmp_decl = cshift.variableDeclaration( 'const', [ cshift.variableDeclarator( cshift.identifier( tmp_var_name ), ktry_arg ) ] );
            const condition = cshift.callExpression( cshift.memberExpression( cshift.identifier( tmp_var_name ), cshift.identifier( 'has_error' ) ), [] );
            const ret_result = cshift.returnStatement( cshift.callExpression( cshift.memberExpression( cshift.identifier( tmp_var_name ), cshift.identifier( 'error' ) ), [] ) );
            const if_stmt = cshift.ifStatement( condition, cshift.blockStatement( [ ret_result ] ) );
            const tmp_value_call = cshift.callExpression( cshift.memberExpression( cshift.identifier( tmp_var_name ), cshift.identifier( 'value' ) ), [] );
            const new_assign = cshift.assignmentStatement( assignment.operator, cshift.identifier( lhs.name ), tmp_value_call );// [ cshift.variableDeclarator( cshift.identifier( decl_name ), tmp_value_call ) ] );

            return cshift.blockStatement( [ tmp_decl, if_stmt, new_assign ] );
        } ); 
    // return ktry( <expr> );
    collection.find( cshift.ReturnStatement, { argument: { type: 'CallExpression'
                                                         , callee: { name: 'ktry' } } } )
        .replaceWith( ( ktry_node_path ) => {
            console.log( 'in RETURN KTRY CASE' );
            const ktry_call = ktry_node_path.node.argument;
            const ktry_args = ktry_call.arguments;
            const [ ktry_body ] = ktry_args;

            const temp_id = cshift.identifier( 'ktry_postproc_temp_val' );
            const res_decl = cshift.variableDeclaration( 'const', [ cshift.variableDeclarator( temp_id, ktry_body ) ] );
            const condition_call = cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'has_error' ) ), [] );
            const error_call = cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'error' ) ), [] );
            const value_call = cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'value' ) ), [] );
            const return_error = cshift.returnStatement( error_call );
            const return_value = cshift.returnStatement( value_call );
            const if_stmt = cshift.ifStatement( condition_call, cshift.blockStatement( [ return_error ] ), cshift.blockStatement( [ return_value ] ) );

            return cshift.blockStatement( [ res_decl, if_stmt ] );
        } );
    // <func>( ktry( < expr > ) )
    collection.find( cshift.CallExpression, { arguments: [ { type: 'CallExpression'
                                                           , callee: { name: 'ktry' } } ] } )
        .replaceWith( ( ktry_node_path ) => {
            const outer_fn = ktry_node_path.node;
            const outer_fn_id = outer_fn.callee.name; 
            const ktry_args = ktry_node_path.node.arguments[ 0 ].arguments;
            const [ ktry_body ] = ktry_args;

            const temp_id = cshift.identifier( 'ktry_postproc_temp_val' );
            const res_decl = cshift.variableDeclaration( 'const', [ cshift.variableDeclarator( temp_id, ktry_body ) ] );
            const condition = cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'has_error' ) ), [] );
            const ret_result = cshift.returnStatement( cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'error' ) ), [] ) );
            const if_stmt = cshift.ifStatement( condition, cshift.blockStatement( [ ret_result ] ) );
            const outer_fn_call = cshift.callExpression( cshift.identifier( outer_fn_id ), [ cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'value' ) ), [] ) ] );

            return cshift.blockStatement( [ res_decl
                                          , if_stmt
                                          , cshift.expressionStatement( outer_fn_call ) ] );
        } );
    // ktry( <expr> );
    collection.find( cshift.CallExpression, { callee: { name: 'ktry' } } )
        .replaceWith( ( ktry_node_path ) => {
            const ktry_args = ktry_node_path.node.arguments;
            const [ ktry_body ] = ktry_args;

            const temp_id = cshift.identifier( 'ktry_postproc_temp_val' );
            const res_decl = cshift.variableDeclaration( 'const', [ cshift.variableDeclarator( temp_id, ktry_body ) ] );
            const condition = cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'has_error' ) ), [] );
            const ret_result = cshift.returnStatement( cshift.callExpression( cshift.memberExpression( temp_id, cshift.identifier( 'error' ) ), [] ) );
            const if_stmt = cshift.ifStatement( condition, cshift.blockStatement( [ ret_result ] ) );

            return cshift.blockStatement( [ res_decl, if_stmt ] );
        } ); 

    return collection.toSource();
}

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
        const options = { 'esversion': 8 // Just guessing on the EMCAScript version. May need to bump in the future.
                        , 'laxcomma': true  // Allows for `, line_item` style of line breaking.
                        , 'laxbreak': true // Allows for `|| line_item` style of line breaking.
                        , '-W032': true // Allow for '{};' trailing semicolon, especially to facilitate output of ktry() transformation.
                        }; 

        // if( !jshint( '/* jshint -W032 */\n' + code, options ) )
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

/// Emscripten Represents C++ exceptions by pointer to linear address / number.
/// Only guaranteed to work when 'err_obj' is the object passed to catch().
function is_cpp_exception( err_obj )
{
    return typeof err_obj === 'number';
}

function clear_text_area()
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) );

        tv.value = "";
    }
    catch( err )
    {
        console.log( String( err ) );
    }
}

function sleep( ms )
{
    return new Promise( resolve => setTimeout( resolve, ms ) );
}

function write_text_area( text )
{
    try
    {
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) );

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
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ) );

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
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) );

        tv.focus();
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
        document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) ).innerHTML = text;
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
        let tv = document.getElementById( kmap.uuid_to_string( kmap.canvas().preview_pane() ) );

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

function debounce( fn, timer_name, timeout )
{
    if( kmap.flags.debounce )
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
    else
    {
        return fn;
    }
}
