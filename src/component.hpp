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
// #include "event/event_clerk.hpp"
#include "kmap.hpp"
#include "util/macro.hpp"

#include <emscripten/bind.h>

#include <any>
#include <compare>
#include <memory>
#include <set>
#include <string_view>

namespace kmap
{
    class Kmap;
}

// TODO: I have a hesitance about supplying the requisites manually, in that it burdens the developer with keeping track.
//       I wonder if there is a way to leverage the "clerk"-system, such that clerks, on creation, register their respective requisite. Bit of a tall order, but a thought.
// Note: Using the simple register_component_<line> as a function name should be generalizable, so long as REGISTER_COMPONENT is called from an anonymous namespace.
#define REGISTER_COMPONENT( type, reqs, desc ) \
    namespace binding \
    { \
        auto KMAP_CONCAT( register_component_, __LINE__ )() \
        { \
            KM_RESULT_PROLOG(); \
                KM_RESULT_PUSH( "id", type::id ); \
            auto& kmap = kmap::Singleton::instance(); \
            struct KMAP_CONCAT( component_ctor, __LINE__ ) : public kmap::ComponentConstructor \
            { \
                using ComponentConstructor::ComponentConstructor; \
                virtual auto construct( kmap::Kmap& km ) const -> std::shared_ptr< kmap::Component > override { return std::static_pointer_cast< kmap::Component >( std::make_shared< type >( km, requisites(), description() ) ); } \
                virtual ~KMAP_CONCAT( component_ctor, __LINE__ )() = default; \
            }; \
            auto cctor = std::make_shared< KMAP_CONCAT( component_ctor, __LINE__ ) >( type::id, reqs, desc ); \
            auto bptr = std::static_pointer_cast< kmap::ComponentConstructor >( cctor ); \
            KTRYE( kmap.component_store().register_component( bptr ) ); \
        } \
        EMSCRIPTEN_BINDINGS( kmap_component ) \
        { \
            emscripten::function( fmt::format( "register_component_{}", kmap::format_heading( type::id ) ).c_str(), &KMAP_CONCAT( register_component_, __LINE__ ) ); \
        } \
    }

namespace kmap {

auto fetch_listed_components()
    -> Result< std::set< std::string > >;
auto register_components( std::set< std::string > const& components )
    -> Result< void >;
auto register_all_components()
    -> Result< void >;

/**
 * There's a problem with destruction and dependence. Currently, a Component is constructed - but not initialized - until its dependencies are resolved.
 * Then it is initialized. Fine, I suppose. A bit disjointed in terms of construction and initialization and destruction, but otherwise ok.
 * The trouble comes when destructing in e.g., EventClerk, in whose dtor the EventStore component is expected to exist. But EventStore may be destructed before
 * the Component holding an EventClerk. Even uninitialized, a destructor will still be called, and unless that component has been initialized and is not destroyed before,
 * fetch_component<>() will fail.
 * The only solution to this problem that I see is to not construct until initialization. This does not mean that initialize() is fully redundant, as it is still useful
 * for returning error results, as ctors can only throw.
 * It means separating a Component from its requisites(). Rather, it doesn't need to deprive a Component of it, but in addition to.
 * { name, requisites, Type }. But how exactly do I communicate the type without construction?
 * In this case, the uninitialized is actually Starter/Constructor which when "initializing", `construct()`s and `initialize`()s.
 * Thereby, no destructor will be called for uninitialized. But what if initialize() fails...? Well, it is still considered constructed, and therefore its requisites should have
 * been initialized already, so destruction should still be safe.
 */
class Component
{
    Kmap& kmap_;
    std::set< std::string > requisites_;
    std::string description_;

public:
    Component( Kmap& kmap // TODO: is Kmap essential to Component?
             , std::set< std::string > const& requisites
             , std::string const& description );
    virtual ~Component() = default;

    [[ nodiscard ]]
    constexpr virtual auto name() const -> std::string_view = 0;
    virtual auto initialize() -> Result< void > = 0;
    virtual auto load() -> Result< void > = 0;

    auto kmap_inst()
        -> Kmap&;
    auto kmap_inst() const
        -> Kmap const&;
    auto description() const
        -> std::string const&;
    auto requisites() const
        -> std::set< std::string > const&;

    template< typename Component >
    auto fetch_component()
    {
        return kmap_inst().fetch_component< Component >();
    }
    template< typename Component >
    auto fetch_component() const
    {
        return kmap_inst().fetch_component< Component >();
    }
};

class ComponentConstructor
{
    std::string name_;
    std::set< std::string > requisites_;
    std::string description_;

public:
    ComponentConstructor( std::string const& name
                        , std::set< std::string > const& requisites
                        , std::string const& description )
        : name_{ name }
        , requisites_{ requisites }
        , description_{ description }
    {
    }

    auto name() const
        -> std::string const&
    {
        return name_;
    }
    auto description() const
        -> std::string const&
    {
        return description_;
    }
    auto requisites() const
        -> std::set< std::string > const
    {
        return requisites_;
    }

    // std::shared_ptr< T > type = std::make_shared< T >{};
    virtual auto construct( Kmap& kmap ) const -> std::shared_ptr< Component > = 0; // This means that each component needs a Starter and Component. Maybe the Starter can be automated...
};

} // namespace kmap

#endif // KMAP_COMPONENT_HPP
