/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_OPTION_OPTION__CLERK_HPP
#define KMAP_OPTION_OPTION_CLERK_HPP

#include "common.hpp"
#include "com/option/option.hpp"

#include <set>
#include <string_view>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {

class OptionClerk
{
    Kmap& kmap_;
    std::map< std::string, Uuid > installed_options_ = {};
    std::map< std::string, Option > registered_options_ = {};

public:
    OptionClerk( Kmap& km );
    ~OptionClerk();

    auto apply_installed()
        -> Result< void >;
    auto check_registered()
        -> Result< void >;
    auto install_registered()
        -> Result< void >;
    auto register_option( Option const& option )
        -> Result< void >;

protected:
    auto install_option( Option const& option )
        -> Result< Uuid >;
};

} // namespace kmap::com

#endif // KMAP_OPTION_OPTION_CLERK_HPP
