/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_OPTION_OPTION_HPP
#define KMAP_OPTION_OPTION_HPP

#include "common.hpp"
#include "kmap.hpp"

#include <string_view>

namespace kmap
{

class OptionStore
{
public:
    OptionStore( Kmap& kmap );

    auto apply( Uuid const& option )
        -> Result< void >;
    auto apply_all()
        -> Result< void >;
    auto option_root()
        -> Uuid;
    auto install_option( Heading const& heading
                       , std::string const& descr
                       , std::string const& value 
                       , std::string const& action  )
        -> Result< Uuid >;

private:
    Kmap& kmap_;
};

} // namespace kmap

#endif // KMAP_OPTION_OPTION_HPP
