/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_SIGNAL_EXCEPTION_HPP
#define KMAP_UTIL_SIGNAL_EXCEPTION_HPP

#include <exception>
#include <string>

namespace kmap {

class SignalException : public std::exception
{
};

class SignalLoadException : public SignalException
{
    std::string db_path_;

public:
    SignalLoadException( std::string const& db_path )
        : db_path_{ db_path }
    {
    }

    auto db_path() -> std::string const { return db_path_; };
    const char* what() const noexcept override { return "exception meant to signal a load operation - not as error"; }
};


} // namespace kmap

#endif // KMAP_UTIL_SIGNAL_EXCEPTION_HPP