/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EVENT_EVENT_HPP
#define KMAP_EVENT_EVENT_HPP

#include "common.hpp"
#include "kmap.hpp"

#include <set>
#include <string_view>

namespace kmap
{

class EventStore
{
public:
    EventStore( Kmap& kmap );

    auto event_root()
        -> Uuid;
    auto event_root() const
        -> Result< Uuid >;

    auto install_defaults()
        -> Result< void >;
    auto install_verb( Heading const& heading )
        -> Result< Uuid >;
    auto install_object( Heading const& heading )
        -> Result< Uuid >;
    auto install_subject( Heading const& heading )
        -> Result< Uuid >;
    auto install_outlet( std::string const& heading
                       , std::string const& desc
                       , std::string const& action 
                       , std::set< std::string > const& requisites ) // List of actions and objects that activate this outlet.
        -> Result< Uuid >;
    auto uninstall_outlet( std::string const& heading )
        -> Result< void >;

    auto action( std::string const& heading )
        -> Result< Uuid >;
    auto object( std::string const& heading )
        -> Result< Uuid >;

    auto execute_body( Uuid const& node )
        -> Result< void >;
    auto fire_event( std::set< std::string > const& requisites )
        -> Result< void >;

private:
    Kmap& kmap_;
};

namespace event
{
    struct any_of;
    struct all_of;
    struct none_of;

    using Variant = std::variant< any_of
                                , all_of
                                , none_of
                                , std::string >;
    struct any_of
    {
        std::vector< Variant > v_;

        // add "any." prefix
        template< typename... Args >
        any_of( Args&&... args )
        {
            ( v_.emplace_back( std::forward< Args >( args ) ), ... );
        }
    };
    struct all_of
    {
        std::vector< std::string > v_;

        // add "any." prefix
        template< typename... Args >
        all_of( Args&&... args )
        {
            ( v_.emplace_back( std::forward< Args >( args ) ), ... );
        }
    };
    struct none_of
    {
        std::vector< std::string > v_;

        // add "any." prefix
        template< typename... Args >
        none_of( Args&&... args )
        {
            ( v_.emplace_back( std::forward< Args >( args ) ), ... );
        }
    };
}

} // namespace kmap

#endif // KMAP_EVENT_EVENT_HPP
