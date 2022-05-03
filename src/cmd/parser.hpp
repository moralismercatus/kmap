/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_PARSER_HPP
#define KMAP_CMD_PARSER_HPP

#include "../common.hpp"

#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

#include <string>
#include <string_view>

namespace kmap::cmd::ast {

namespace x3 = boost::spirit::x3;

struct Kscript : x3::position_tagged
{
    std::string code;
};

struct Javascript : x3::position_tagged
{
    std::string code;
};

struct Code : x3::position_tagged
            , x3::variant< Kscript
                         , Javascript >
{
    using base_type::base_type;
    using base_type::operator=;
};

} // namespace kmap::cmd::ast

namespace kmap::cmd::parser {

auto parse_body_code( std::string_view const raw )
    -> Result< cmd::ast::Code >;

} // namespace kmap::cmd::parser

#endif // KMAP_CMD_PARSER_HPP