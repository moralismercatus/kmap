/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_JUMP_STACK_HPP
#define KMAP_JUMP_STACK_HPP

#include "com/event/event_clerk.hpp"
#include "com/option/option_clerk.hpp"
#include "common.hpp"
#include "component.hpp"

#include <deque>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class JumpStack : public Component
{
    using Stack = std::deque< Uuid >;

    EventClerk eclerk_;
    OptionClerk oclerk_;
    std::deque< Uuid > buffer_ = {};
    unsigned threshold_ = 100u;

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
    auto register_standard_options()
        -> Result< void >;

    auto build_pane_table()
        -> Result< void >;

    auto is_adjacent( Uuid const& n1
                    , Uuid const& n2 )
        -> bool;
    auto push_transition( Uuid const& from
                        , Uuid const& to )
        -> void;
    auto jump_out()
        -> Optional< Uuid >;
    auto set_threshold( unsigned const& max )
        -> void;
    auto stack() const
        -> Stack const&;
};

} // namespace kmap::com

#endif // KMAP_JUMP_STACK_HPP
