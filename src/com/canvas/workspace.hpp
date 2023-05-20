/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_WORKSPACE_HPP
#define KMAP_CANVAS_WORKSPACE_HPP

#include <com/canvas/common.hpp>
#include <com/canvas/pane_clerk.hpp>
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
class Workspace : public Component
{
    PaneClerk pclerk_;

public:
    static constexpr auto id = "canvas.workspace";
    constexpr auto name() const -> std::string_view override { return id; }

    Workspace( Kmap& km
          , std::set< std::string > const& requisites
          , std::string const& description );
    virtual ~Workspace() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_panes()
        -> Result< void >;

};

} // kmap::com

#endif // KMAP_CANVAS_WORKSPACE_HPP
