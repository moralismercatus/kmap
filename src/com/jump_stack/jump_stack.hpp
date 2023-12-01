/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_JUMP_STACK_HPP
#define KMAP_JUMP_STACK_HPP

#include <com/canvas/pane_clerk.hpp>
#include <com/event/event_clerk.hpp>
#include <com/option/option_clerk.hpp>
#include <com/cmd/cclerk.hpp>
#include <common.hpp>
#include <component.hpp>

#include <deque>
#include <optional>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class JumpStack : public Component
{
public:
    struct StackItem
    {
        Uuid id;
        std::string label;
    };
    using Stack = std::deque< StackItem >;

private:
    EventClerk eclerk_;
    OptionClerk oclerk_;
    PaneClerk pclerk_;
    CommandClerk cclerk_;
    Stack buffer_ = {};
    Stack::size_type threshold_ = 100u;
    std::optional< Stack::size_type > active_item_index_ = std::nullopt;
    bool ignore_transitions_ = false;

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
    auto register_panes()
        -> Result< void >;
    auto register_commands()
        -> Result< void >;

    auto build_pane_table()
        -> Result< void >;

    auto active_item_index() const
        -> std::optional< Stack::size_type >;
    auto clear_jump_in_items()
        -> void;
    auto format_cell_label( Uuid const& node )
        -> Result< std::string >;
    auto is_adjacent( Uuid const& n1
                    , Uuid const& n2 )
        -> bool;
    auto jump_in()
        -> Result< bool >;
    auto jump_out()
        -> Result< bool >;
    auto purge_nonexistent()
        -> void;
    auto push_transition( Uuid const& from
                        , Uuid const& to )
        -> Result< bool >;
    auto threshold( Stack::size_type const& max )
        -> void;
    auto threshold() const
        -> unsigned;
    auto stack() const
        -> Stack const&;
    auto update_pane()
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_JUMP_STACK_HPP
