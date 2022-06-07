/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TEXT_AREA_HPP
#define KMAP_TEXT_AREA_HPP

#include "common.hpp"

#include <string>

namespace kmap
{
class Kmap;

class TextArea
{
public:
    TextArea( Kmap& kmap );

    auto clear()
        -> void;
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

private:
    Kmap& kmap_;
};

} // namespace kmap

#endif // KMAP_TEXT_AREA_HPP
