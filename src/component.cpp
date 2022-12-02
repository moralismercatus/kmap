/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "component.hpp"

#include "com/event/event_clerk.hpp"
#include "error/result.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove_if.hpp>

#include <any>

namespace kmap {

auto fetch_listed_components()
    -> Result< std::set< std::string > >
{
    using emscripten::val;
    using emscripten::vecFromJSArray;

    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::set< std::string > >();

    KMAP_ENSURE( js::is_global_kmap_valid(), error_code::common::uncategorized );

    auto const components = vecFromJSArray< std::string >( val::global( "kmap_components" ) );

    rv = components | ranges::to< std::set >();

    return rv;
}

auto register_components( std::set< std::string > const& components )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    for( auto const& component : components )
    {
        if( auto const& succ = js::eval_void( fmt::format( "kmap.register_component_{}();", format_heading( component ) ) )
          ; !succ )
        {
            fmt::print( stderr
                      , "failed to register {}\n"
                      , component );
        }
    }

    rv = outcome::success();

    return rv;
}

auto register_all_components()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KMAP_ENSURE( js::is_global_kmap_valid(), error_code::common::uncategorized );

    auto const components = KTRY( fetch_listed_components() );

    KTRY( register_components( components ) );

    rv = outcome::success();

    return rv;
}

Component::Component( Kmap& kmap
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : kmap_{ kmap }
    , requisites_{ requisites }
    , description_{ description }
{
}

auto Component::kmap_inst()
    -> Kmap&
{
    return kmap_;
}

auto Component::kmap_inst() const
    -> Kmap const&
{
    return kmap_;
}

auto Component::description() const
    -> std::string const&
{
    return description_;
}

auto Component::requisites() const
    -> std::set< std::string > const&
{
    return requisites_;
}

} // namespace kmap