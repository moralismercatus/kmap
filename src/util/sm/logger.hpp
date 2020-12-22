/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_SM_LOGGER_HPP
#define KMAP_UTIL_SM_LOGGER_HPP

#include "../../common.hpp"

#include <boost/sml.hpp>

namespace kmap {

struct SmlLogger
{
    template< class Event >
    auto to_string( Event const& ev ) -> std::string
    {
        if constexpr( requires{ ev.to_string(); } )
        {
            return '(' + ev.to_string() + ')';
        }
        else
        {
            return "";
        }
    };

    template< class SM, class TEvent >
    void log_process_event( TEvent const& ev )
    {
        using namespace boost;

        io::print( "[{}]\n\t[process_event]{}{}\n"
                 , sml::aux::get_type_name< SM >()
                 , sml::aux::get_type_name< TEvent >()
                 , this->to_string( ev ) );
    }

    template< class SM, class TGuard, class TEvent >
    void log_guard( TGuard const&, TEvent const&, bool result )
    {
        using namespace boost;

        io::print( "[{}]\n\t[guard]{} {} {}\n"
                 , sml::aux::get_type_name< SM >()
                 , sml::aux::get_type_name< TGuard >()
                 , sml::aux::get_type_name< TEvent >()
                 , ( result ? "[OK]" : "[Reject]" ) );
    }

    template< class SM, class TAction, class TEvent >
    void log_action( TAction const&, TEvent const& ) 
    {
        using namespace boost;

        io::print( "[{}]\n\t[action]{} {}\n"
                 , sml::aux::get_type_name< SM >()
                 , sml::aux::get_type_name< TAction >()
                 , sml::aux::get_type_name< TEvent >() );
    }

    template< class SM, class TSrcState, class TDstState >
    void log_state_change( TSrcState const& src, TDstState const& dst )
    {
        using namespace boost;

        io::print( "[{}]\n\t[transition]{} -> {}\n"
                 , sml::aux::get_type_name< SM >()
                 , src.c_str()
                 , dst.c_str() );
    }
};

} // namespace kmap

#endif // KMAP_UTIL_SM_LOGGER_HPP