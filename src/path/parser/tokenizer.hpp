/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_TOKENIZER_HPP
#define KMAP_PATH_TOKENIZER_HPP

#include <common.hpp>

#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

namespace kmap::ast::path {

namespace x3 = boost::spirit::x3;

// TODO: It isn't necessary to store the delim, but I don't know how to make Boost.Spirit push an element without pushing the char.
//       Providing the char value_type is a temporary workaround.
struct BwdDelim : x3::position_tagged
{
    using value_type = char;
    value_type value;
};
struct DisAmbigDelim : x3::position_tagged
{
    using value_type = char;
    value_type value;
};
struct FwdDelim : x3::position_tagged
{
    using value_type = char;
    value_type value;
};

struct Heading : x3::position_tagged
{
    using value_type = std::string;
    value_type value;
};

struct Tag : x3::position_tagged
{
    using value_type = std::string;
    value_type value;
};

struct Element : x3::position_tagged
               , x3::variant< BwdDelim
                            , DisAmbigDelim
                            , FwdDelim
                            , Heading
                            , Tag >
{
    using base_type::base_type;
    using base_type::operator=;
};

struct HeadingPath : x3::position_tagged
{
    using value_type = std::vector< Element >;
    value_type value;
};

} // kmap::ast::path

namespace kmap::parser::path {

auto tokenize_heading_path( std::string_view const raw )
    -> Result< ast::path::HeadingPath >;

} // namespace kmap::parser::path

#endif // KMAP_PATH_TOKENIZER_HPP