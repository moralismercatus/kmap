/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_JS_SCOPED_CODE_HPP
#define KMAP_JS_SCOPED_CODE_HPP

#include <string>

namespace kmap::js {

class ScopedCode
{
    std::string ctor_code;
    std::string dtor_code;

public:
    ScopedCode() = default;
    ScopedCode( std::string const& ctor
              , std::string const& dtor );
    ScopedCode( ScopedCode&& other );
    ScopedCode( ScopedCode const& other ) = delete;
    ~ScopedCode();

    auto operator=( ScopedCode&& other ) -> ScopedCode&;
    auto operator=( ScopedCode const& other ) -> ScopedCode& = delete;
};

} // namespace kmap::js

#endif // KMAP_JS_SCOPED_CODE_HPP
