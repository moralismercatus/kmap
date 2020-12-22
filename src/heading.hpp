/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_HEADING_HPP
#define KMAP_HEADING_HPP

#include "common.hpp"

#include <string_view>
#include <string>

namespace kmap
{

class Heading;
class HeadingPath;
using HeadingVec = std::vector< Heading >;

/// Alternative constructor for Heading. Returns false if Heading cannot be constructed from 's'.
auto to_heading( std::string_view const s )
    -> Optional< Heading >;
/// Alternative constructor for HeadingPath. Returns false if HeadingPath cannot be constructed from 's'.
auto to_heading_path( std::string_view const s )
    -> Optional< HeadingPath >;
auto is_valid_heading_char( char const c )
    -> bool;
auto is_valid_heading( std::string_view const s )
    -> bool;
auto begin( Heading& )
    -> std::string::iterator;
auto cbegin( Heading const& )
    -> std::string::const_iterator;
auto end( Heading& )
    -> std::string::iterator;
auto cend( Heading const& )
    -> std::string::const_iterator;
auto begin( HeadingPath& )
    -> HeadingVec::iterator;
auto cbegin( HeadingPath const& )
    -> HeadingVec::const_iterator;
auto end( HeadingPath& )
    -> HeadingVec::iterator;
auto cend( HeadingPath const& )
    -> HeadingVec::const_iterator;

class Heading 
{
public:
    Heading();
    Heading( std::string_view const s );

    // Conversions
    auto to_string() const
        -> std::string;
    operator std::string() const;
    operator HeadingPath() const;
    // Access
    auto begin() // TODO: I seriously doubt this function should be allowed. The primary purpose of this class is to ensure that the underling string is constrained to be only a valid heading. Allowing user write access to the internals breaks this guarantee.
        -> std::string::iterator;
    auto begin() const
        -> std::string::const_iterator;
    auto end()
        -> std::string::iterator;
    auto end() const
        -> std::string::const_iterator;

private:
    std::string heading_;
};

class HeadingPath
{
public:
    HeadingPath();
    HeadingPath( std::string_view const s );

    // Conversions.
    auto to_string() const
        -> std::string;
    operator std::string() const;
    // Access
    auto begin()
        -> HeadingVec::iterator;
    auto cbegin() const
        -> HeadingVec::const_iterator;
    auto end()
        -> HeadingVec::iterator;
    auto cend() const
        -> HeadingVec::const_iterator;

private:
    HeadingVec headings_;
};

} // namespace kmap

#endif // KMAP_HEADING_HPP
