/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TASK_HPP
#define KMAP_TASK_HPP

#include "common.hpp"
#include "event/event_clerk.hpp"

namespace kmap {

auto fetch_task_root( Kmap& kmap )
    -> Result< Uuid >;

// Needs to be a made into a `kmap.component`.
class TaskStore
{
    Kmap& kmap_;
    event::EventClerk eclerk_;

public:
    TaskStore( Kmap& kmap );

    auto cascade_tags( Uuid const& task )
        -> Result< void >;
    auto create_subtask( Uuid const& supertask
                       , std::string const& title )
        -> Result< Uuid >;
    auto create_task( std::string const& title )
        -> Result< Uuid >;

    auto is_result_documented( Uuid const& task )
        -> bool;
    auto is_categorized( Uuid const& task )
        -> bool;
    auto is_task( Uuid const& node )
        -> bool;

    auto close_task( Uuid const& node )
        -> Result< void >;
    auto open_task( Uuid const& node )
        -> Result< void >;
};

} // namespace kmap

#endif // KMAP_TASK_HPP
