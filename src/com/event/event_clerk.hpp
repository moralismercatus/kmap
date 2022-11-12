/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EVENT_EVENT_CLERK_HPP
#define KMAP_EVENT_EVENT_CLERK_HPP

#include "common.hpp"
#include "com/event/event.hpp"

#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace kmap::com {

// TODO: So, yet another problem...
//       What if a common requisite is created by this clerk? I.e., "verb.created". When this destructs,
//       it will erase "verb.created", yet it is common, so others depend on it. Suddenly, it's gone!
//       Proposal:
//       1. Somehow maintain a shared_ptr-like mechanism, so each concerned clerk/party is holding onto a counted-ref.
//       1. Provide a "fire" routine here. The "fire" routine then pre-checks each requisite for existence, creating it if it doesn't exist.
//          It then claims ownership if created. That might be the easiest solution....
//          Perhaps the same mechanism can be done for outlets, alleviating the need for an explicit call to "install_default_events()"....
class EventClerk
{
public:
    Kmap& kmap;
    std::map< std::string, com::Transition > registered_outlets = {};
    std::vector< Uuid > subjects = {};
    std::vector< Uuid > verbs = {};
    std::vector< Uuid > objects = {};
    std::vector< Uuid > components = {};
    std::vector< Uuid > outlets = {};
    std::vector< Uuid > outlet_transitions = {};
    std::optional< std::string > payload = std::nullopt;
    std::set< std::string > outlet_com_requisites = {};

    EventClerk( Kmap& km );
    EventClerk( Kmap& km
              , std::set< std::string > const& outlet_com_reqs ); // TODO: Rather template< Args... >( Component::id... )? That is, supplying Component classes.
    ~EventClerk();

    auto append_com_reqs( std::set< std::string > const& requisites )
        -> std::set< std::string >;
    auto append_com_reqs( Leaf const& leaf )
        -> Leaf;
    auto append_com_reqs( Branch const& branch )
        -> Branch;
    auto check_registered()
        -> Result< void >;
    auto check_registered( Leaf const& leaf )
        -> Result< void >;
    auto check_registered( Branch const& branch )
        -> Result< void >;
    auto check_registered( std::string const& heading )
        -> Result< void >;
    auto fire_event( std::set< std::string > const& requisites )
        -> Result< void >;
    auto fire_event( std::set< std::string > const& requisites
                   , std::string const& payload )
        -> Result< void >;
    auto install_subject( Heading const& heading )
        -> Result< Uuid >;
    auto install_verb( Heading const& heading )
        -> Result< Uuid >;
    auto install_object( Heading const& heading )
        -> Result< Uuid >;
    auto install_component( Heading const& heading )
        -> Result< Uuid >;
    auto install_outlet_transition( Uuid const& root
                                  , com::Transition const& transition )
        -> Result< void >;
    auto register_outlet( com::Transition const& t ) 
        -> void;
    auto install_registered()
        -> Result< void >;
    auto install_requisites( std::set< std::string > const& requisites )
        -> Result< void >;

protected:
    auto install_outlet( com::Leaf const& leaf ) 
        -> Result< Uuid >;
    auto install_outlet( com::Branch const& branch ) 
        -> Result< Uuid >;
    auto install_outlet( std::string const& path ) 
        -> Result< Uuid >;
};

auto gather_requisites( Transition const& t )
    -> std::set< std::string >;
auto is_action_consistent( Kmap const& km
                         , Uuid const& lnode
                         , std::string const& content )
    -> bool;
auto is_description_consistent( Kmap const& km
                              , Uuid const& lnode
                              , std::string const& content )
    -> bool;
auto match_requisites( Kmap const& km
                     , Uuid const eroot
                     , Uuid const& lnode
                     , std::set< std::string > const& alias_paths )
    -> bool;

} // namespace kmap::com

#endif // KMAP_EVENT_EVENT_CLERK_HPP
