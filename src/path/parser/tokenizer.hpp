/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_TOKENIZER_HPP
#define KMAP_PATH_TOKENIZER_HPP

#include <common.hpp>

#include <boost/optional.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/variant/recursive_wrapper.hpp>

#include <compare>

namespace kmap::ast::path {

namespace x3 = boost::spirit::x3;

struct BwdDelim;
struct DisAmbigDelim;
struct FwdDelim;
struct Heading;
struct Tag;

// TODO: It isn't necessary to store the delim, but I don't know how to make Boost.Spirit push an element without pushing the char.
//       Providing the char value_type is a temporary workaround.
struct BwdDelim : x3::position_tagged
{
    using child_type = x3::variant< boost::recursive_wrapper< BwdDelim >
                                  , x3::forward_ast< DisAmbigDelim >
                                  , x3::forward_ast< FwdDelim >
                                  , x3::forward_ast< Heading >
                                  , x3::forward_ast< Tag > >;
    using value_type = char;
    value_type value = {};
    boost::optional< child_type > child = boost::none;
};
struct DisAmbigDelim : x3::position_tagged
{
    using child_type = x3::variant< x3::forward_ast< Heading >
                                  , x3::forward_ast< Tag > >;
    using value_type = char;
    value_type value = {};
    boost::optional< child_type > child = boost::none;
};
struct FwdDelim : x3::position_tagged
{
    using child_type = x3::variant< x3::forward_ast< BwdDelim >
                                  , x3::forward_ast< DisAmbigDelim >
                                  , boost::recursive_wrapper< FwdDelim >
                                  , x3::forward_ast< Heading >
                                  , x3::forward_ast< Tag > >;
    using value_type = char;
    value_type value = {};
    boost::optional< child_type > child = boost::none;
};

struct Tag : x3::position_tagged
{
    using child_type = x3::variant< x3::forward_ast< BwdDelim >
                                  , x3::forward_ast< DisAmbigDelim >
                                  , x3::forward_ast< FwdDelim >
                                  , boost::recursive_wrapper< Tag > >;
    using value_type = std::string;
    value_type value = {};
    boost::optional< child_type > child = boost::none;
};

struct Heading : x3::position_tagged
{
    using child_type = x3::variant< x3::forward_ast< BwdDelim >
                                  , x3::forward_ast< DisAmbigDelim >
                                  , x3::forward_ast< FwdDelim >
                                  , x3::forward_ast< Tag > >;
    using value_type = std::string;
    value_type value = {};
    boost::optional< child_type > child = boost::none;
};

struct HeadingPath : x3::position_tagged
                   , x3::variant< BwdDelim
                                , DisAmbigDelim
                                , FwdDelim
                                , Heading
                                , Tag >
{
    using base_type::base_type;
    using base_type::operator=;
};

auto to_string( ast::path::HeadingPath const& hp )
    -> std::string; 
auto flatten( ast::path::HeadingPath const& hp )
    -> std::vector< ast::path::HeadingPath >;

} // kmap::ast::path

namespace kmap::parser::path {

auto tokenize_heading_path( std::string_view const raw )
    -> Result< ast::path::HeadingPath >;

} // namespace kmap::parser::path

#endif // KMAP_PATH_TOKENIZER_HPP