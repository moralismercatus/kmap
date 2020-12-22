/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TEXT_VIEW_HPP
#define KMAP_TEXT_VIEW_HPP

#include <string>

namespace kmap
{

class TextView
{
public:
    auto clear()
        -> void;
    auto write( std::string const& text )
        -> void;
    auto focus_editor()
        -> void;
    auto focus_preview()
        -> void;
    auto editor_contents()
        -> std::string;
    auto hide_editor()
        -> void;
    auto show_editor()
        -> void;
    auto resize_editor( std::string const& attr )
        -> void;
    auto show_preview( std::string const& text )
        -> void;
    auto hide_preview()
        -> void;
    auto resize_preview( std::string const& attr )
        -> void;

private:
};

} // namespace kmap

#endif // KMAP_TEXT_VIEW_HPP
