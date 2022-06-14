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

class TaskStore
{
    Kmap& kmap_;
    event::EventClerk eclerk_;

public:
    TaskStore( Kmap& kmap );

    auto create_task( std::string const& title )
        -> Result< Uuid >;
};

} // namespace kmap

#endif // KMAP_TASK_HPP
