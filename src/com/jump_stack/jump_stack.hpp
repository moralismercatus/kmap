/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_JUMP_STACK_HPP
#define KMAP_JUMP_STACK_HPP

#include "com/event/event_clerk.hpp"
#include "common.hpp"
#include "component.hpp"

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class JumpStack : public Component
{
    EventClerk eclerk_;

public:
    static constexpr auto id = "jump_stack";
    constexpr auto name() const -> std::string_view override { return id; }

    JumpStack( Kmap& km
             , std::set< std::string > const& requisites
             , std::string const& description );
    virtual ~JumpStack() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_event_outlets()
        -> Result< void >;

    // TODO: I think these probably don't belong? Jump stack reacts to selection events.
    auto jump_in( Uuid const& node )
        -> void;//Optional< Uuid >;
    auto jump_out()
        -> Optional< Uuid >;
protected:
};

} // namespace kmap::com

#endif // KMAP_JUMP_STACK_HPP
