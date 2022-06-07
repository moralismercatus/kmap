/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CHRONO_TIMER_HPP
#define KMAP_CHRONO_TIMER_HPP

#include "common.hpp"
#include "event/event_clerk.hpp"

#include <set>

namespace kmap
{
    class Kmap;
}

namespace kmap::chrono {

class Timer
{
    using TimerID = uint64_t;

    Kmap& kmap_;
    event::EventClerk eclerk_;
    std::set< TimerID > timer_ids_ = {};

public:
    Timer( Kmap& kmap );
    ~Timer();

    auto install_default_timers()
        -> Result< void >;
    auto install_timer( std::string const& requisite_js_list
                      , uint32_t const& ms_frequency )
        -> Result< void >;
};

} // namespace kmap::chrono

#endif // KMAP_EVENT_EVENT_HPP
