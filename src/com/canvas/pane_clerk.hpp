/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CANVAS_PANE_CLERK_HPP
#define KMAP_CANVAS_PANE_CLERK_HPP

#include <common.hpp>
#include <com/canvas/common.hpp>

#include <map>
#include <set>
#include <string_view>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

// TODO: Along with this, need the master layout.js JSON consulted. If the pane isn't found existent, consult the layout.
// TODO: The other unresolved aspect is how to specify a minimum distance. Perhaps don't allow a pane to have less than a 0.05 size.
class PaneClerk
{
    Kmap& kmap_;
    std::map< std::string, Uuid > installed_panes_ = {};
    std::map< std::string, Uuid > installed_overlays_ = {};
    std::map< std::string, Pane > registered_panes_ = {};
    std::map< std::string, Overlay > registered_overlays_ = {};

public:
    PaneClerk( Kmap& km );
    ~PaneClerk();

    auto check_registered()
        -> Result< void >;
    auto install_registered()
        -> Result< void >;
    auto register_pane( Pane const& pane )
        -> Result< void >;
    auto register_overlay( Overlay const& overlay )
        -> Result< void >;

protected:
    auto ensure_html_element_exists( Uuid const& id )
        -> Result< void >;
    auto install_pane( Pane const& pane )
        -> Result< Uuid >;
    auto install_overlay( Overlay const& overlay )
        -> Result< Uuid >;
};

} // namespace kmap::com

#endif // KMAP_CANVAS_PANE_CLERK_HPP
