/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <js/iface.hpp>

#include <contract.hpp>
#include <emcc_bindings.hpp>
#include <error/js_iface.hpp>
#include <util/result.hpp>
#include <utility.hpp>
#include <test/util.hpp>

#include <boost/algorithm/string.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace ranges;

namespace kmap::js {

auto append_child_element( std::string const& parent_doc_id
                         , std::string const& child_doc_id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "parent_doc_id", parent_doc_id );
        KM_RESULT_PUSH_STR( "child_doc_id", child_doc_id );

    auto rv = result::make_result< void >();

    KMAP_ENSURE( element_exists( parent_doc_id ), error_code::js::invalid_element );
    KMAP_ENSURE( element_exists( child_doc_id ), error_code::js::invalid_element );

    KTRY( js::eval_void( io::format( "let parent = document.getElementById( '{}' ); console.assert( parent );"
                                     "let child = document.getElementById( '{}' ); console.assert( child );"
                                     "let res = parent.appendChild( child ); console.assert( res );"
                                   , parent_doc_id
                                   , child_doc_id ) ) );

    rv = outcome::success();

    return rv;
}

// TODO: Should probably de-constrain and rename to: create_html_elem_child( id=<'uuid'>, tag=<'e.g., body'> )
auto create_html_canvas( std::string const& id )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "id", id );

    auto rv = result::make_result< void >();

    KMAP_ENSURE( !element_exists( id ), error_code::js::element_already_exists );

    KTRY( js::eval_void( io::format( "let canvas = document.createElement( 'canvas' );"
                                     "canvas.id = '{}';"
                                     "let body_tag = document.getElementsByTagName( 'body' )[ 0 ];"
                                     "body_tag.appendChild( canvas );" 
                                   ,  id ) ) );

    rv = outcome::success();

    return rv;
}

auto create_child_element( std::string const& parent_id
                         , std::string const& child_id
                         , std::string const& elem_type )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "parnet_id", parent_id );
        KM_RESULT_PUSH( "child_id", child_id );
        KM_RESULT_PUSH( "elem_type", elem_type );

    auto rv = result::make_result< void >();

    KTRY( js::eval_void( io::format( "let child = document.createElement( '{}' );"
                                     "child.id = '{}';"
                                     "let parent = document.getElementById( '{}' );"
                                     "parent.appendChild( child );" 
                                   , elem_type
                                   , child_id
                                   , parent_id ) ) );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "doc_id", doc_id );

    auto rv = result::make_result< void >();

    KMAP_ENSURE( element_exists( doc_id ), error_code::js::invalid_element );

    KTRY( js::eval_void( io::format( "let elem = document.getElementById( '{}' ); console.assert( elem );"
                                     "let res = elem.parentNode.removeChild( elem ); console.assert( res );"
                                   , doc_id ) ) );

    rv = outcome::success();

    return rv;
}

auto fetch_parent_element_id( std::string const& doc_id )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "doc_id", doc_id );

    auto rv = result::make_result< std::string >();

    KMAP_ENSURE( element_exists( doc_id ), error_code::js::invalid_element );

    rv = KTRY( js::eval< std::string >( io::format( "let elem = document.getElementById( '{}' ); console.assert( elem );"
                                                    "let parent = elem.parentElement; console.assert( parent );"
                                                    "return parent.id;"
                                                  , doc_id ) ) );

    return rv;
}

auto lint( std::string const& code )
    -> Result< void >
{
    using emscripten::val;

    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >(); 

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

auto move_element( std::string const& src_doc_id
                 , std::string const& dst_doc_id )
    -> Result< void >
{    
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "src_doc_id", src_doc_id );
        KM_RESULT_PUSH_STR( "dst_doc_id", dst_doc_id );

    auto rv = result::make_result< void >();

    KTRY( js::eval_void( io::format( "let child = document.getElementById( '{}' ); console.assert( child );"
                                     "let old_parent = child.parentElement; console.assert( old_parent );"
                                     "let new_parent = document.getElementById( '{}' ); console.assert( new_parent );"
                                     "old_parent.removeChild( child );"
                                     "new_parent.appendChild( child );"
                                   , src_doc_id
                                   , dst_doc_id ) ) );

    rv = outcome::success();

    return rv;
}

SCENARIO( "js::move_element", "[js_iface]" )
{
    using emscripten::val;

    GIVEN( "html.body" )
    {
        REQUIRE_TRY( js::eval< val >( "return document.body;" ) );

        auto const body_id = to_string( gen_uuid() );

        REQUIRE_TRY( js::eval_void( fmt::format( "document.body.id='{}';", body_id ) ) );

        GIVEN( "body.appendChild( 'e1' )" )
        {
            auto const e1_id = to_string( gen_uuid() );

            REQUIRE_TRY( js::create_child_element( body_id, e1_id, "div" ) );

            GIVEN( "body.appendChild( 'e2' )" )
            {
                auto const e2_id = to_string( gen_uuid() );

                REQUIRE_TRY( js::create_child_element( body_id, e2_id, "div" ) );

                WHEN( "move_element( e1, e2 )" )
                {
                    REQUIRE_TRY( js::move_element( e1_id, e2_id ) );

                    THEN( "e1 child of e2" )
                    {
                        REQUIRE( js::is_child_element( e2_id, e1_id ) );
                    }
                }
            }
        }
    }
}

auto eval_val( std::string const& expr )
    -> Result< emscripten::val >
{
    using emscripten::val;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "expr", expr );

    auto rv = KMAP_MAKE_RESULT( val ); 
    auto constexpr script =
R"%%%(
(
    function()
    {{
        try
        {{
            {}
        }}
        catch( err )
        {{
            if( is_cpp_exception( err ) )
            {{
                if( kmap.is_signal_exception( err ) )
                {{
                    console.log( 'signal exception recieved' );
                    throw err;
                }}
                else
                {{
                    console.error( '[kmap][error] std::exception encountered:' );
                    // print std exception
                    kmap.print_std_exception( err );
                }}
            }}
            else // Javascript exception
            {{
                console.error( '\n[kmap][error] JS exception: ' + err + '\n' );
            }}
            return undefined;
        }}
    }}
)();
)%%%";
    auto const eval_str = io::format( script, expr );

    KMAP_ENSURE( lint( eval_str ), error_code::js::lint_failed );

    KTRY( set_last_eval_code_var( expr ) );

    auto const eval_res = val::global().call< val >( "eval", eval_str );

    KMAP_ENSURE_MSG( !eval_res.isNull(), error_code::js::eval_failed, "Null returned (did you fail to return a result?) failed to evaluate javascript expression" );
    KMAP_ENSURE_MSG( !eval_res.undefined(), error_code::js::eval_failed, "Undefined returned (did you fail to return a result?) failed to evaluate javascript expression" );

    auto const payload_ref = val::global( "kmap" )[ "result$$Payload" ];
    KMAP_ENSURE( payload_ref.as< bool >(), error_code::js::eval_failed )

    if( eval_res.instanceof( payload_ref ) )
    {
        fmt::print( "instanceof payload ref!!\n" );
        auto const payload = eval_res.as< error_code::Payload >();
        rv = payload;
    }
    else
    {
        rv = eval_res; 
    }

    return rv;
}

SCENARIO( "eval_val comment handling", "[js][js_iface]" )
{
    REQUIRE_TRY( eval_val( "console.log( 'test' ); // Just a test" ) );
    REQUIRE_TRY( eval_val( "// Just a test" ) );
    REQUIRE_TRY( eval_val( "/* Just a test */" ) );
}

// Considerable overlap between eval_void and eval< T >, no? Shouldn't eval_void just call eval< T >?
// I don't think so. It would be nice, but I don't know how to return "eval_result" with an eval< T > and check it in a convenient way,
// to see if the returned result was an error, or a successful result of the given expression.
// If only I knew a way to query val type e.g., if( v.is_type< Result >() ){ ... } rather than the system failing if it cannot bind to the type.
auto eval_void( std::string const& expr )
    -> Result< void >
{
    using emscripten::val;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "expr", expr );

    auto rv = result::make_result< void >(); 
    auto const nexpr = fmt::format( "{}\nreturn {}", expr, "kmap.eval_success();" );
    auto const eval_res = KTRY( eval_val( nexpr ) );
    auto const eval_succ_ref = val::global( "kmap" )[ "EvalSuccess" ];

    KMAP_ENSURE( eval_succ_ref.as< bool >(), error_code::js::eval_failed )
    KMAP_ENSURE( eval_res.instanceof( eval_succ_ref ), error_code::js::eval_failed )

    rv = outcome::success(); 

    return rv;
}

SCENARIO( "eval_void comment handling", "[js][js_iface]" )
{
    REQUIRE_TRY( eval_val( "console.log( 'test' ); // Just a test" ) );
    REQUIRE_TRY( eval_val( "// Just a test" ) );
    REQUIRE_TRY( eval_val( "/* Just a test */" ) );
}

auto preprocess( std::string const& code )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();

    // fmt::print( "pre: {}\n", code );

    rv = KTRY( js::call< std::string >( "kmap_preprocess_js_script", code ) );

    // fmt::print( "post: {}\n", rv.value() );

    return rv;
}

auto publish_function( std::string_view const name
                     , StringVec const& params
                     , std::string_view const body )
    -> Result< void >
{
    using emscripten::val;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "name", std::string{ name } );

    auto rv = result::make_result< void >();

    KMAP_ENSURE( lint( std::string{ body } ), error_code::js::lint_failed );

    auto const joined_params = params
                             | views::join( ',' )
                             | to< std::string >();
    auto const pp = KTRY( js::preprocess( std::string{ body } ) );
    auto const fn_def = fmt::format( "global.{} = function( {} ) {{\n{}\n}};"
                                   , name
                                   , joined_params 
                                   , pp );

    rv = eval_void( fn_def );

    return rv;
}

auto is_child_element( std::string const& parent_id
                     , std::string const& child_id )
    -> bool
{
    KM_RESULT_PROLOG();

    return KTRYE( eval< bool >( fmt::format( "return document.getElementById( '{}' ).parentElement.id == '{}';"
                                           , child_id
                                           , parent_id ) ) );
}

auto is_global_kmap_valid()
    -> bool 
{
    KM_RESULT_PROLOG();

    return KTRYE( eval< bool >( "return ( global.kmap !== undefined ) && ( global.kmap !== null );" ) );
}

auto fetch_style_member( std::string const& elem_id )
    -> Result< emscripten::val >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "elem_id", elem_id );

    return KTRY( eval< emscripten::val >(io::format( "return document.getElementById( '{}' ).style;", elem_id ).c_str() ) );
}

auto set_global_kmap( Kmap& kmap )
    -> void
{
    using emscripten::val;

    KM_RESULT_PROLOG();

    KTRYE( js::eval_void( "global.kmap = Module;" ) );

    if( is_global_kmap_valid() ) // TODO: Figure out how to make it so kmap can't be reassigned.
    {
        fmt::print( "kmap module set\n" );
    }
    else
    {
        fmt::print( stderr, "Unable to set kmap module\n" );
    }
}

auto set_last_eval_code_var( std::string const& expr )
    -> Result< void >
{
    using emscripten::val;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "expr", expr );

    // Note: "kmap.js_last_eval_code" used to convey what JS code failed evaluation after non-try-catch-able to `.onerror` handler.
    auto expr_escaped = replace_unescaped_char( expr, '`', R"(\\`)" );
    expr_escaped = replace_unescaped_char( expr_escaped, '$', R"(\\$)" );
    auto const script = fmt::format( "kmap.js_last_eval_code = `{}`;", expr_escaped );

    KMAP_ENSURE( lint( script ), error_code::js::lint_failed );

    auto const eval_res = val::global().call< val >( "eval", script );

    return outcome::success();
}

auto set_tab_index( std::string const& elem_id
                  , unsigned const& index )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "elem_id", elem_id );

    auto rv = result::make_result< void >();

    KTRY( js::eval_void( io::format( "let elem = document.getElementById( '{}' ); console.assert( elem );"
                                     "elem.tabIndex = {};"
                                   , elem_id
                                   , index ) ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::js

namespace kmap::js::binding {

struct EvalSuccess{};

auto make_eval_success()
    -> EvalSuccess
{
    return {};
}

} // kmap::js::binding

using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_js_binding )
{
    class_< kmap::js::binding::EvalSuccess >( "EvalSuccess" );
    function( "eval_success", &kmap::js::binding::make_eval_success );
}
