/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "js_iface.hpp"

#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "error/js_iface.hpp"
#include "utility.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace ranges;

namespace kmap::js {

// TODO: Should probably de-constrain and rename to: create_html_elem_child( id=<'uuid'>, tag=<'e.g., body'> )
auto create_html_canvas( std::string const& id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( !element_exists( id ), error_code::js::invalid_element );

    KMAP_TRY( js::eval_void( io::format( "let canvas = document.createElement( 'canvas' );"
                                         "canvas.id = '{}';"
                                         "let body_tag = document.getElementsByTagName( 'body' )[ 0 ];"
                                         "body_tag.appendChild( canvas );" 
                                       ,  id ) ) ); 

    rv = outcome::success();

    return rv;
}

auto element_exists( std::string const& doc_id )
    -> bool
{
    return fetch_element_by_id< emscripten::val >( doc_id ).has_value();
}

auto exists( Uuid const& id )
    -> bool
{
    return element_exists( to_string( id ) );
}

auto erase_child_element( std::string const& doc_id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( element_exists( doc_id ), error_code::js::invalid_element );

    KMAP_TRY( js::eval_void( io::format( "let elem = document.getElementById( '{}' ); console.assert( elem );"
                                         "let res = elem.parentNode.removeChild( elem ); console.assert( res );"
                                       , doc_id ) ) );

    rv = outcome::success();

    return rv;
}

auto lint( std::string const& code )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void ); 

    if( auto const linted = call< std::string >( "lint_javascript", code )
      ; linted )
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::js::lint_failed, linted.value() );
    }
    else
    {
        rv = outcome::success();
    }

    return rv;
}

// Considerable overlap between eval_void and eval< T >, no? Shouldn't eval_void just call eval< T >?
// I don't think so. It would be nice, but I don't know how to return "eval_result" with an eval< T > and check it in a convenient way,
// to see if the returned result was an error, or a successful result of the given expression.
// If only I knew a way to query val type e.g., if( v.is_type< Result >() ){ ... } rather than the system failing if it cannot bind to the type.
auto eval_void( std::string const& expr )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void ); 

    KMAP_ENSURE( lint( expr ), error_code::js::lint_failed );

    auto constexpr script =
R"%%%(
(
    function()
    {{
        try
        {{
            {}
            return kmap.eval_success();
        }}
        catch( err )
        {{
            if( is_cpp_exception( err ) )
            {{
                console.log( '[kmap][error] std::exception encountered:' );
                // print std exception
                kmap.print_std_exception( err );
                return kmap.eval_failure( kmap.std_exception_to_string( err ) );
            }}
            else // Javascript exception
            {{
                console.log( '[kmap][error] JS exception: ' + err );
                return kmap.eval_failure( String( err ) );
            }}
        }}
        return undefined; // Should never reach.
    }}
)();
)%%%";
    // auto const eval_str = io::format( "( function(){{ try{{ {} return kmap.eval_success(); }} catch( err ){{ console.log( err ); return kmap.eval_failure( String( err ) ); }} }} )()"
    //                                 , expr );
    auto const eval_str = io::format( script, expr );

    if( auto const res = val::global().call< val >( "eval", eval_str )
      ; !res.isUndefined() && !res.isNull() )
    {
        if( auto const except = res.as< binding::Result< void > >()
          ; except.has_error() )
        {
            rv = KMAP_PROPAGATE_FAILURE( except.result );
        }
        else
        {
            rv = outcome::success();
        }
    }

    return rv;
}

auto publish_function( std::string_view const name
                     , StringVec const& params
                     , std::string_view const body )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( lint( std::string{ body } ), error_code::js::lint_failed );

    auto const joined_params = params
                             | views::join( ',' )
                             | to< std::string >();
    auto const fn_def = fmt::format( "global.{} = function( {} ) {{ {} }};"
                                   , name
                                   , joined_params 
                                   , body );

    rv = eval_void( fn_def );

    return rv;
}

auto is_global_kmap_null()
    -> bool 
{
    using emscripten::val;

    return val::global().call< bool >( "eval", std::string{ "global.kmap != null" } );
}

auto set_global_kmap( Kmap& kmap )
    -> void
{
    using emscripten::val;

    // TODO: Shouldn't this be a call to js::eval_void?
    val::global().call< val >( "eval"
                             , std::string{ "global.kmap = Module;" } );

    if( is_global_kmap_null() ) // TODO: Figure out how to make it so kmap can't be reassigned.
    {
        fmt::print( "kmap module set\n" );
    }
    else
    {
        fmt::print( stderr, "Unable to set kmap module\n" );
    }
}

} // namespace kmap::js
