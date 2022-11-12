/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TEXT_AREA_HPP
#define KMAP_TEXT_AREA_HPP

#include "com/cmd/cclerk.hpp"
#include "com/option/option_clerk.hpp"
#include "com/event/event_clerk.hpp"
#include "common.hpp"
#include "component.hpp"
#include "js_iface.hpp"

#include <string>
#include <vector>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class TextArea : public Component
{
    com::EventClerk eclerk_;
    std::vector< js::ScopedCode > scoped_events_ = {};
    com::CommandClerk cclerk_;
    option::OptionClerk oclerk_;

public:
    static constexpr auto id = "text_area";
    constexpr auto name() const -> std::string_view override { return id; }

    TextArea( Kmap& kmap
            , std::set< std::string > const& requisites
            , std::string const& description );
    virtual ~TextArea() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto apply_static_options()
        -> Result< void >;
    auto clear()
        -> void;
    auto register_standard_commands()
        -> void;
    auto register_standard_outlets()
        -> void;
    auto install_event_sources()
        -> Result< void >;
    auto install_standard_options()
        -> Result< void >;
    auto load_preview( Uuid const& node )
        -> Result< void >;
    auto set_editor_text( std::string const& text )
        -> void;
    auto focus_editor()
        -> void;
    auto focus_preview()
        -> void;
    auto editor_contents()
        -> std::string;
    auto hide_editor()
        -> Result< void >;
    auto show_editor()
        -> Result< void >;
    auto rebase_pane( float const base )
        -> void;
    auto rebase_preview_pane( float const base )
        -> void;
    auto rebase_editor_pane( float const base )
        -> void;
    auto show_preview( std::string const& text )
        -> Result< void >;
    auto hide_preview()
        -> Result< void >;
    auto resize_preview( std::string const& attr )
        -> void;
    auto update_pane()
        -> void;
};

} // namespace kmap::com

#endif // KMAP_TEXT_AREA_HPP
