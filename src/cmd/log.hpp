/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_LOG_HPP
#define KMAP_CMD_LOG_HPP

#include "../cli.hpp"

#include <functional>
#include <chrono>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto fetch_or_create_daily_log( Kmap& kmap
                              , std::string const& date )
    -> Optional< Uuid >;
auto fetch_daily_log( Kmap const& kmap
                    , std::string const& date )
    -> Optional< Uuid >;

} // namespace kmap::cmd

#endif // KMAP_CMD_LOG_HPP
