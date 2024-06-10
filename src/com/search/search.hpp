/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_SEARCH_SEARCH_HPP
#define KMAP_SEARCH_SEARCH_HPP

#include <com/cmd/cclerk.hpp>
#include <com/option/option_clerk.hpp>
#include <common.hpp>
#include <component.hpp>

#include <set>
#include <string>

namespace kmap::com {

// Version 1:
// This will register a :search.titles command that accepts a text input of arbitrary size and content.
// It will open an interactive overlay consisting of a table with columns for matching title and body.
// Focus goes to the overlay. User can traverse, select "enter" node. I don't think showing heading will be necessary.
// The overlay is dismissed with ctrl+c/esc.
// The fuzzy search backend is in a stable state, albeit not great at what it does, but no matter. That can change in the future without problem.
// Version 2,3,...
// It should fairly trivial to replicate the functionality for other fuzzy results such as bodies, specific nodes.
class Search : public Component
{
    CommandClerk cclerk_;
    OptionClerk oclerk_;

public:
    static constexpr auto id = "search";
    constexpr auto name() const -> std::string_view override { return id; }

    Search( Kmap& kmap
          , std::set< std::string > const& requisites
          , std::string const& description );
    virtual ~Search() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto search_titles( std::string const& query )
        -> Result< void >;

    auto register_standard_commands()
        -> Result< void >;
    auto register_standard_options()
        -> Result< void >;
};


} // namespace kmap::com

#endif // KMAP_SEARCH_SEARCH_HPP
