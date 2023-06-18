/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_LAYOUT_HPP
#define KMAP_CANVAS_LAYOUT_HPP

#include <com/canvas/common.hpp>
#include <com/option/option_clerk.hpp>
#include <common.hpp>
#include <component.hpp>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kmap::com {

/**
 * It should be noted that subdivisions, and their hierarchies, are represented as nodes, so all actions except for "update_*" change the node
 * description, and not the HTML itself. It is for "update" to apply these representations to the HTML.
 * 
 */
class Layout : public Component
{
    OptionClerk oclerk_;

public:
    static constexpr auto id = "canvas.layout";
    constexpr auto name() const -> std::string_view override { return id; }

    Layout( Kmap& km
          , std::set< std::string > const& requisites
          , std::string const& description );
    virtual ~Layout() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_options()
        -> Result< void >;
};

} // kmap::com

#endif // KMAP_CANVAS_LAYOUT_HPP
