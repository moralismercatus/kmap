/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "js_iface.hpp"

#include "contract.hpp"
#include "error/js_iface.hpp"
#include "utility.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace ranges;

namespace kmap::js {

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

auto eval_void( std::string const& expr )
    -> Result< void >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( void ); 

    KMAP_ENSURE( rv, lint( expr ), error_code::js::lint_failed );

    auto const eval_str = io::format( "( function(){{ try{{ {} return true; }} catch( err ){{ console.log( err ); return undefined; }} }} )()"
                                    , expr );

    if( auto const res = val::global().call< val >( "eval"
                                                  , eval_str )
      ; !res.isUndefined() && !res.isNull() )
    {
        rv = outcome::success();
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

    KMAP_ENSURE( rv, lint( std::string{ body } ), error_code::js::lint_failed );

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

} // namespace kmap::js
