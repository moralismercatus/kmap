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

namespace kmap::option {

struct OptionClerk
{
    Kmap& kmap;
    std::vector< Uuid > options = {};

    OptionClerk( Kmap& km );
    ~OptionClerk();

    auto install_option( Heading const& heading
                       , std::string const& descr
                       , std::string const& value 
                       , std::string const& action )
    -> Result< Uuid >;
};

} // namespace kmap::option

#endif // KMAP_OPTION_OPTION_CLERK_HPP
