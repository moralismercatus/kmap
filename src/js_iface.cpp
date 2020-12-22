/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "js_iface.hpp"

#include "contract.hpp"
#include "utility.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace ranges;

namespace kmap::js
{
// TODO: A canvas represents what's rendered inside the window on screen. Thus do TextView, Editor, Network, breadcrumbs, CLI, belong here?

auto publish_function( std::string_view const name
                     , StringVec const& params
                     , std::string_view const body )
    -> bool
{
    using emscripten::val;

    auto rv = false;
    auto const joined_params = params
                             | views::join( ',' )
                             | to< std::string >();
    auto const fn_def = fmt::format( "global.{} = function( {} ) {{ {} }};"
                                   , name
                                   , joined_params 
                                   , body );

    if( auto const fn_created = val::global().call< val >( "eval_code"
                                                         , fn_def )
      ; fn_created.as< bool >() )
    {
        rv = true;
    }

    return rv;
}

} // namespace kmap::js
