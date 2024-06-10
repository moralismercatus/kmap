/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TASK_HPP
#define KMAP_TASK_HPP

#include <com/cmd/cclerk.hpp>
#include <com/event/event_clerk.hpp>
#include <com/tag/tag.hpp>
#include <common.hpp>
#include <component.hpp>
#include <path/node_view2.hpp>

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
    auto activate_task( Uuid const& node )
        -> Result< void >;
    auto deactivate_task( Uuid const& node )
        -> Result< void >;
    
    auto register_standard_commands()
        -> void;
};

auto is_order_regular( Kmap const& km ) // Could be com::Network
    -> bool;
auto order_tasks( Kmap const& km )
    -> Result< UuidVec >;

} // namespace kmap

namespace kmap::view2::task
{
    auto const task_root = anchor::abs_root | view2::direct_desc( "task" );
    auto const task = view2::child( view2::all_of( view2::child, { "problem", "result" } ) ); // TODO: Factor is_categorized into the equation.
    auto const active_tag = view2::tag::tag_root | view2::direct_desc( "task.status.open.active" );
    auto const inactive_tag = view2::tag::tag_root | view2::direct_desc( "task.status.open.inactive" );
    auto const closed_tag = view2::tag::tag_root | view2::direct_desc( "task.status.closed" );
} // kmap::view2::task

#endif // KMAP_TASK_HPP
