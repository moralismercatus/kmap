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
