/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COM_ROOT_NODE_HPP
#define KMAP_COM_ROOT_NODE_HPP

#include <common.hpp>
#include <component.hpp>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

constexpr auto root_node_welcome_text = "Welcome.\n\nType `:help` for assistance.";

// TODO: Belongs in network rather than db.
class RootNode : public Component
{
    Uuid root_ = {};

public:
    static constexpr auto id = "root_node";
    constexpr auto name() const -> std::string_view override { return id; }

    using Component::Component;
    virtual ~RootNode() = default; // TODO: Shouldn't the dtor erase the root, as this component "owns" it?

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto root_node() const
        -> Uuid const&;
};

} // namespace kmap::com

#endif // KMAP_COM_ROOT_NODE_HPP
