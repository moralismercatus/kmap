/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

#pragma once
#ifndef KMAP_CLI_PARSER_HPP
#define KMAP_CLI_PARSER_HPP

#include <common.hpp>

#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

namespace kmap::ast::cli {

namespace x3 = boost::spirit::x3;

struct Heading : x3::position_tagged
{
    using value_type = std::string;
    value_type heading;
};

struct Title : x3::position_tagged
{
    using value_type = std::string;
    value_type title;
};

struct SoloCommand : x3::position_tagged
{
    using value_type = std::string;
    value_type command;
};

struct CommandArgPair : x3::position_tagged
{
    std::string command;
    std::string arg;
};

struct Command : x3::position_tagged
               , x3::variant< SoloCommand
                            , CommandArgPair >
{
    using base_type::base_type;
    using base_type::operator=;
};

struct SoloSelection : x3::position_tagged
{
    using value_type = Heading;
    value_type selection;
};

struct SelectionCommandPair : x3::position_tagged
{
    Heading selection;
    Command command;
};

struct Selection : x3::position_tagged
                 , x3::variant< SoloSelection
                              , SelectionCommandPair >
{
    using base_type::base_type;
    using base_type::operator=;
};

struct CommandBar : x3::position_tagged
                  , x3::variant< Selection
                               , Command >
{
    using base_type::base_type;
    using base_type::operator=;
};

} // kmap::ast::cli

namespace kmap::parser::cli {

auto parse_command_bar( std::string_view const raw )
    -> Result< ast::cli::CommandBar >;

auto fetch_selection_string( ast::cli::CommandBar const& cbar )
    -> Optional< std::string >;
auto fetch_command_string( ast::cli::CommandBar const& cbar )
    -> Optional< std::pair< std::string, std::string > >;

} // namespace kmap::parser::cli

#endif //  KMAP_CLI_PARSER_HPP