/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_COM_LOG_HPP
#define KMAP_COM_LOG_HPP

#include "com/cli/cli.hpp"
#include "error/master.hpp"

#include <date/date.h>

#include <functional>
#include <chrono>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

auto fetch_or_create_daily_log( Kmap& kmap
                              , std::string const& date )
    -> Result< Uuid >;
auto fetch_or_create_daily_log( Kmap& kmap )
    -> Result< Uuid >;
auto fetch_daily_log( Kmap const& kmap
                    , std::string const& date )
    -> Result< Uuid >;
auto fetch_daily_log( Kmap const& kmap )
    -> Result< Uuid >;
template< typename TimePoint >
auto to_log_date_string( TimePoint const& tp )
    -> std::string
{
    using namespace date;

    io::print( "[warning] TODO: impl. local time zone for date string\n" );

    return date::format( "%Y.%m.%d", tp );
}
auto present_date_string()
    -> std::string;
auto present_daily_log_path()
    -> std::string;

} // namespace kmap::com

#endif // KMAP_COM_LOG_HPP
