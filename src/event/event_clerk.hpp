/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EVENT_EVENT_CLERK_HPP
#define KMAP_EVENT_EVENT_CLERK_HPP

#include "common.hpp"
#include "event/event.hpp"

#include <set>
#include <string_view>

namespace kmap::event {

// TODO: So, yet another problem...
//       What if a common requisite is created by this clerk? I.e., "verb.created". When this destructs,
//       it will erase "verb.created", yet it is common, so others depend on it. Suddenly, it's gone!
//       Proposal:
//       1. Somehow maintain a shared_ptr-like mechanism, so each concerned clerk/party is holding onto a counted-ref.
//       1. Provide a "fire" routine here. The "fire" routine then pre-checks each requisite for existence, creating it if it doesn't exist.
//          It then claims ownership if created. That might be the easiest solution....
//          Perhaps the same mechanism can be done for outlets, alleviating the need for an explicit call to "install_default_events()"....
struct EventClerk
{
    Kmap& kmap;
    std::vector< Uuid > subjects = {};
    std::vector< Uuid > verbs = {};
    std::vector< Uuid > objects = {};
    std::vector< Uuid > outlets = {};
    std::vector< Uuid > outlet_transitions = {};

    EventClerk( Kmap& km );
    ~EventClerk();

    auto fire_event( std::vector< std::string > const& requisites )
        -> Result< void >;
    auto install_subject( Heading const& heading )
        -> Result< Uuid >;
    auto install_verb( Heading const& heading )
        -> Result< Uuid >;
    auto install_object( Heading const& heading )
        -> Result< Uuid >;
    auto install_outlet( Leaf const& leaf ) 
        -> Result< void >;
    auto install_outlet( Branch const& branch ) 
        -> Result< void >;
    auto install_outlet_transition( Uuid const& root
                                  , Transition const& transition )
        -> Result< void >;
};

} // namespace kmap::event

#endif // KMAP_EVENT_EVENT_CLERK_HPP
