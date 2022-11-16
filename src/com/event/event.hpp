/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EVENT_EVENT_HPP
#define KMAP_EVENT_EVENT_HPP

#include "common.hpp"
#include "component.hpp"

#include <memory>
#include <optional>
#include <set>
#include <string_view>

namespace kmap::com { // TODO: kmap::event?

struct Debounce
{
    std::string action = {};
    uint32_t timeout = {};
};
struct Leaf;
struct Branch;
using Transition = std::variant< Leaf, Branch >;
struct Leaf
{
    std::string heading;
    std::set< std::string > requisites;
    std::string description;
    std::string action;
    // std::variant< std::string, Debounce > action;
};
struct Branch
{
    std::string heading;
    std::set< std::string > requisites;
    std::vector< Transition > transitions;
};

class EventStore : public Component
{
    std::vector< Transition > registered_outlets_ = {};
    struct TransitionState
    {
        UuidSet active;
    };
    // So, how this works is as follows:
    // Every outlet part of an outlet tree gets placed into this map, and all point to the same shared_ptr.
    // Only the active state gets acted upon.
    using TransitionMap = std::map< Uuid, std::shared_ptr< TransitionState > >;
    TransitionMap transition_states_;
    std::optional< std::string > payload_ = std::nullopt;

public:
    static constexpr auto id = "event_store";
    constexpr auto name() const -> std::string_view override { return id; }

    using Component::Component;
    virtual ~EventStore() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto event_root()
        -> Result< Uuid >;
    auto event_root() const
        -> Result< Uuid >;
    
    auto fetch_outlet_base( Uuid const& node )
        -> Result< Uuid >;
    auto fetch_outlet_tree( Uuid const& outlet )
        -> Result< UuidSet >;
    auto fetch_matching_outlets( std::set< std::string > const& requisites )
        -> Result< UuidSet >;
    auto fetch_payload()
        -> Result< std::string >;

    auto install_verb( Heading const& heading )
        -> Result< Uuid >;
    auto install_object( Heading const& heading )
        -> Result< Uuid >;
    auto install_subject( Heading const& heading )
        -> Result< Uuid >;
    auto install_component( Heading const& heading )
        -> Result< Uuid >;
    // auto install_outlet( std::string const& prefix_path
    //                    , Transition const& transition ) 
    //     -> Result< Uuid >;
    auto install_outlet( Leaf const& leaf ) 
        -> Result< Uuid >;
    auto install_outlet( Branch const& branch ) 
        -> Result< Uuid >;
    auto install_outlet_transition( Uuid const& root
                                  , Transition const& transition )
        -> Result< Uuid >;
    auto is_active_outlet( Uuid const& outlet )
        -> bool;
    auto is_outlet( Uuid const& node )
        -> bool;
    auto register_outlet( Transition const& transition )
        -> Result< void >;
    auto uninstall_subject( std::string const& heading )
        -> Result< void >;
    auto uninstall_subject( Uuid const& node )
        -> Result< void >;
    auto uninstall_verb( std::string const& heading )
        -> Result< void >;
    auto uninstall_verb( Uuid const& node )
        -> Result< void >;
    auto uninstall_object( std::string const& heading )
        -> Result< void >;
    auto uninstall_object( Uuid const& node )
        -> Result< void >;
    auto uninstall_component( Uuid const& node )
        -> Result< void >;
    auto uninstall_outlet( std::string const& heading )
        -> Result< void >;
    auto uninstall_outlet( Uuid const& node )
        -> Result< void >;
    auto uninstall_outlet_transition( std::string const& heading )
        -> Result< void >;
    auto uninstall_outlet_transition( Uuid const& node )
        -> Result< void >;

    auto action( std::string const& heading )
        -> Result< Uuid >;
    auto object( std::string const& heading )
        -> Result< Uuid >;

    auto execute_body( Uuid const& node )
        -> Result< void >;
    auto fire_event( std::set< std::string > const& requisites )
        -> Result< void >;
    auto fire_event( std::set< std::string > const& requisites
                   , std::string const& payload )
        -> Result< void >;

    auto reset_transitions( Uuid const& outlet )
        -> Result< void >;
    auto reset_transitions( std::set< std::string > const& requisites )
        -> Result< void >;

protected:
    auto fire_event_internal( std::set< std::string > const& requisites )
        -> Result< void >;
    auto install_outlet_internal( Uuid const& root 
                                , Leaf const& leaf )
        -> Result< void >;
    auto install_outlet_internal( Uuid const& root 
                                , Branch const& leaf )
        -> Result< void >;
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

} // namespace kmap::com

#endif // KMAP_EVENT_EVENT_HPP