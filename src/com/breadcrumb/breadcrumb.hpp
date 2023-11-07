/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_BREADCRUMB_HPP
#define KMAP_BREADCRUMB_HPP

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

class Breadcrumb : public Component
{
public:
    using Stack = std::deque< Uuid >;

private:
    EventClerk eclerk_;
    OptionClerk oclerk_;
    PaneClerk pclerk_;
    CommandClerk cclerk_;
    Stack buffer_ = {};
    std::optional< Stack::size_type > active_item_index_ = std::nullopt;
    bool ignore_transitions_ = false;

public:
    static constexpr auto id = "breadcrumb";
    constexpr auto name() const -> std::string_view override { return id; }

    Breadcrumb( Kmap& km
             , std::set< std::string > const& requisites
             , std::string const& description );
    virtual ~Breadcrumb() = default;

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
    auto update_pane()
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_BREADCRUMB_HPP
