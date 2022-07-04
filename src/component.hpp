/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COMPONENT_HPP
#define KMAP_COMPONENT_HPP

#include "common.hpp"
#include "emcc_bindings.hpp"
#include "event/event_clerk.hpp"
#include "util/macro.hpp"

#include <emscripten/bind.h>

#include <any>
#include <compare>
#include <memory>

#define REGISTER_COMPONENT( name, desc, requisites, type ) \
    namespace binding \
    { \
        auto KMAP_CONCAT( register_component_, __LINE__ )() \
        { \
            auto& kmap = kmap::Singleton::instance(); \
            KTRYE( kmap.component_store().register_component( kmap::Component{ #name, desc, requisites, ( std::make_shared< type >( kmap ) ) } ) ); \
        } \
        EMSCRIPTEN_BINDINGS( kmap_component ) \
        { \
            emscripten::function( "register_component_"#name, &KMAP_CONCAT( register_component_, __LINE__ ) ); \
        } \
    }

namespace kmap {

class Kmap;

auto register_components()
    -> Result< void >;

struct ComponentBase
{
    virtual auto initialize() -> Result< void > = 0;
    virtual ~ComponentBase() = default;
};

// TODO: What is this for...?
class ComponentClerk
{
    Kmap& kmap_;
    std::string component_name;

public:
    ComponentClerk( std::string const& name );
};

struct Component // TODO: Why not include this info in the Component itself? Part of ComponentBase? Then, this can be removed and ComponentBase can be named Component
{
    std::string name = {};
    std::string description = {};
    std::set< std::string > requisites = {};
    std::shared_ptr< ComponentBase > inst = {}; // TODO: Should probably be unique_ptr that is moved around...

    // std::strong_ordering operator<=>( Component const& ) const = default;
};

class ComponentStore
{
    Kmap& kmap_;
    event::EventClerk eclerk_;
    std::set< std::string > received_inits_ = {};
    std::map< std::string, Component > uninitialized_components_ = {};
    // std::set< Component > initialized_components_ = {};
    // std::map< std::string, std::any > initialized_components_ = {};
    std::map< std::string, Component > initialized_components_ = {};

public:
    ComponentStore( Kmap& kmap );

    // So... when we get an requisite subject.component, we first add it to the received_init_requisites_ set.
    // Then, we check each uninitialized_component to see if its requisites are all in received_init_requisites.
    // If so, we call initialize on the component, and move it to the initialized_components set.
    auto install_default_events()
        -> Result< void >;
    auto register_component( Component const& comp )
        -> Result< void >;
    auto fire_initialized( std::string const& id )
        -> Result< void >;

    template< typename T >
    auto fetch_component( std::string const& id )
        -> Result< T >;
};

} // namespace kmap

#endif // KMAP_COMPONENT_HPP
