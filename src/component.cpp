/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "component.hpp"

#include "error/result.hpp"
#include "event/event_clerk.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#include <any>

namespace kmap {

auto register_components()
    -> Result< void >
{
    using emscripten::val;
    using emscripten::vecFromJSArray;

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( js::is_global_kmap_valid(), error_code::common::uncategorized );

    auto const components = vecFromJSArray< std::string >( val::global( "kmap_components" ) );

    for( auto const& component : components )
    {
        if( auto const& succ = js::eval_void( fmt::format( "kmap.register_component_{}();", component ) )
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

ComponentStore::ComponentStore( Kmap& kmap )
    : kmap_{ kmap }
    , eclerk_{ kmap }
{
}

auto ComponentStore::install_default_events()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    
    KTRY( eclerk_.install_outlet( Leaf{ .heading = "component.register_initialization"
                                      , .requisites = { "subject.component", "verb.initialized" }
                                      , .description = "registers a component to the ComponentStore for dispatching dependent components"
                                      , .action = R"%%%(kmap.component_store().register_component( kmap.event_store().payload() );)%%%" } ) );

    rv = outcome::success();

    return rv;
}

auto ComponentStore::register_component( Component const& comp )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    if( !uninitialized_components_.contains( comp.name ) )
    {
        uninitialized_components_.emplace( comp.name, comp );

        rv = outcome::success();
    }

    return rv;
}

// TODO: Should we require that "id" maps to a registered component?
auto ComponentStore::fire_initialized( std::string const& id )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto new_inits = decltype( uninitialized_components_ ){};

    received_inits_.emplace( id );

    for( auto const& [ name, uc ] : uninitialized_components_ )
    {
        if( ranges::all_of( uc.requisites, [ & ]( auto const& req ){ return received_inits_.contains( req ); } ) )
        {
            new_inits.emplace( name, uc );
        }
    }

    for( auto const& [ name, comp ] : new_inits )
    {
        initialized_components_.emplace( name, comp );

        KTRY( initialized_components_[ name ].inst->initialize() );

        uninitialized_components_.erase( name );
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap

namespace kmap::binding {

struct ComponentStore
{
    auto register_component( std::string const& id )
        -> void;
};

} // namespace kmap::binding

EMSCRIPTEN_BINDINGS( kmap_component )
{
}
