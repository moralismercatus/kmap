/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_EVENT_HPP
#define KMAP_EVENT_HPP

#include <boost/sml.hpp>

#include <memory>

namespace kmap
{

struct Event
{
    std::vector< uint32_t > key_codes = {};
    std::string command = {};
};

struct EventSm;

class EventDispatch final
{
public:
    EventDispatch();

    auto dispatch_key_down( uint64_t const& key )
        -> void;
    auto dispatch_key_up( uint64_t const& key )
        -> void;

private:
    std::unique_ptr< EventSm > sm_;
};

auto is_event( Kmap const& kmap
             , Uuid const& id )
    -> bool;
auto fetch_events( Heading const& path )
    -> std::vector< Event >;

} // namespace kmap

#endif // KMAP_EVENT_HPP
