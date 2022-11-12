/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TASK_HPP
#define KMAP_TASK_HPP

#include "common.hpp"
#include "com/event/event_clerk.hpp"
#include "com/cmd/cclerk.hpp"
#include "component.hpp"

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

auto fetch_task_root( Kmap const& kmap )
    -> Result< Uuid >;
auto fetch_task_root( Kmap& kmap )
    -> Result< Uuid >;

// Needs to be a made into a `kmap.component`.
class TaskStore : public Component
{
    com::EventClerk eclerk_;
    com::CommandClerk cclerk_;

public:
    static constexpr auto id = "task_store";
    constexpr auto name() const -> std::string_view override { return id; }

    TaskStore( Kmap& kmap
             , std::set< std::string > const& requisites
             , std::string const& description );

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto cascade_tags( Uuid const& task )
        -> Result< void >;
    auto create_subtask( Uuid const& supertask
                       , std::string const& title )
        -> Result< Uuid >;
    auto create_task( std::string const& title )
        -> Result< Uuid >;

    auto is_result_documented( Uuid const& task ) const
        -> bool;
    auto is_categorized( Uuid const& task ) const
        -> bool;
    auto is_task( Uuid const& node ) const
        -> bool;

    auto close_task( Uuid const& node )
        -> Result< void >;
    auto open_task( Uuid const& node )
        -> Result< void >;
    
    auto register_standard_commands()
        -> void;
};

} // namespace kmap

#endif // KMAP_TASK_HPP
