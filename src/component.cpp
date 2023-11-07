/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <component.hpp>

#include <com/event/event_clerk.hpp>
#include <error/result.hpp>
#include <kmap.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>
#endif // !KMAP_NATIVE

#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/transform.hpp>

#include <any>

namespace rvs = ranges::views;

namespace kmap {

auto fetch_listed_components()
    -> Result< std::set< std::string > >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::set< std::string > >();

#if !KMAP_NATIVE
    KMAP_ENSURE( js::is_global_kmap_valid(), error_code::common::uncategorized );

    auto const components = emscripten::vecFromJSArray< std::string >( emscripten::val::global( "kmap_components" ) );

    rv = components | ranges::to< std::set >();
#else
    auto const& regs = NativeComponentRegistry::instance();

    rv = regs
       | rvs::transform( []( auto const& e ){ return e.first; } )
       | ranges::to< std::set >();
#endif // !KMAP_NATIVE

    return rv;
}

auto register_components( std::set< std::string > const& components )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

#if !KMAP_NATIVE
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
#else
    auto& km = kmap::Singleton::instance();
    auto& cstore = km.component_store();
    auto const& registry = NativeComponentRegistry::instance();
    for( auto const& component : components )
    {
        KMAP_ENSURE_MSG( registry.contains( component ), error_code::common::uncategorized, component );
        KTRY( cstore.register_component( registry.at( component ) ) );
    }
#endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

auto register_all_components()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

#if !KMAP_NATIVE
    KMAP_ENSURE( js::is_global_kmap_valid(), error_code::common::uncategorized );
#endif // !KMAP_NATIVE

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

#if KMAP_NATIVE
std::unique_ptr< NativeComponentRegistry::Registry > NativeComponentRegistry::inst_ = {};
#endif // KMAP_NATIVE

} // namespace kmap