/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COM_TIMER_HPP
#define KMAP_COM_TIMER_HPP

#include "common.hpp"
#include "component.hpp"
#include "com/event/event_clerk.hpp"

#include <set>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class Timer : public Component
{
    using TimerID = uint64_t;

    com::EventClerk eclerk_;
    std::set< TimerID > timer_ids_ = {};

public:
    static constexpr auto id = "timer";
    constexpr auto name() const -> std::string_view override { return id; }

    Timer( Kmap& kmap
         , std::set< std::string > const& requisites
         , std::string const& description );
    virtual ~Timer();

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto install_default_timers()
        -> Result< void >;
    auto install_timer( std::string const& requisite_js_list
                      , uint32_t const& ms_frequency )
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_COM_TIMER_HPP
