/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COMPONENT_STORE_HPP
#define KMAP_COMPONENT_STORE_HPP

#include "common.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>

namespace kmap
{
    class Kmap;
    class Component;
    class ComponentConstructor;
}

namespace kmap {

class ComponentStore
{
    using ComponentPtr = std::shared_ptr< Component >;
    using ComponentCtorPtr = std::shared_ptr< ComponentConstructor >;
    using ComponentMap = std::map< std::string, ComponentPtr >;

    Kmap& km_;
    // event::EventClerk eclerk_;
    std::set< std::string > received_inits_ = {};
    std::map< std::string, ComponentCtorPtr > uninitialized_components_ = {};
    ComponentMap initialized_components_ = {};

public:
    ComponentStore( Kmap& km );
    ~ComponentStore();

    auto all_uninit_dependents( std::string const& component )
        -> std::set< std::string >;
    auto clear()
        -> Result< void >;
    auto erase_component( ComponentPtr const& com )
        -> Result< void >;
    auto fire_initialized( std::string const& id )
        -> Result< void >;
    auto fire_loaded( std::string const& id )
        -> Result< void >;
    // So... when we get an requisite subject.component, we first add it to the received_init_requisites_ set.
    // Then, we check each uninitialized_component to see if its requisites are all in received_init_requisites.
    // If so, we call initialize on the component, and move it to the initialized_components set.
    auto install_standard_events()
        -> Result< void >;
    auto register_component( ComponentCtorPtr const& comp )
        -> Result< void >;

    // TODO: I'd rather not return a ptr, rather a reference, but is Result< T& > allowed?
    // TODO: std::shared_ptr should not be used. It's important for ComponentStore to have exclusive ownership, such that when the ComponentStore
    //       destructs, all Components are cleaned up. If somehow another source is preserving lifetime (e.g., JS handle), assumptions are broken.
    template< typename T >
    auto fetch_component()
        -> Result< std::shared_ptr< T > >
    {
        auto rv = KMAP_MAKE_RESULT_MSG( std::shared_ptr< T >, fmt::format( "initialized component not found: '{}'\n", T::id ) );
        auto const id = T::id;

        if( initialized_components_.contains( id ) )
        {
            if( auto const com = std::dynamic_pointer_cast< T >( initialized_components_[ id ] )
              ; com )
            {
                rv = com;
            }
        }

        return rv;
    }
    template< typename T >
    auto fetch_component() const // TODO: Rundant w/ non-const version, but what can we do without const_cast or "deducing this (c++23)"?
        -> Result< std::shared_ptr< T const > >
    {
        auto rv = KMAP_MAKE_RESULT( std::shared_ptr< T const > );
        auto const id = T::id;

        if( initialized_components_.contains( id ) )
        {
            if( auto const com = std::dynamic_pointer_cast< T >( initialized_components_.at( id ) )
              ; com )
            {
                rv = com;
            }
        }

        return rv;
    }
};

} // namespace kmap

#endif // KMAP_COMPONENT_STORE_HPP
