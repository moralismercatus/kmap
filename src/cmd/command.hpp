/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_COMMAND_HPP
#define KMAP_CMD_COMMAND_HPP

#include "../error/node_manip.hpp"
#include "util/macro.hpp"

#include "com/cli/cli.hpp"
#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

// auto execute_command( Kmap& kmap )
//     -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto execute_command( Kmap& kmap
                    , Uuid const& cmd_id
                    , std::string const& arg )
    -> Result< void >;
auto execute_kscript( Kmap& kmap 
                    , std::string const& code )
    -> Result< void >;
auto execute_javascript( Uuid const& node
                       , std::string_view const body
                       , StringVec const& args )
    -> Result< void >;
[[ nodiscard ]]
auto evaluate_guard( Kmap const& kmap
                   , Uuid const& guard_node
                   , std::string const& arg )
    -> Result< void >;
[[ nodiscard ]]
auto evaluate_completer( Kmap& kmap
                       , Uuid const& completer_node
                       , std::string const& arg )
    -> Result< StringVec >;
[[ nodiscard ]]
auto has_unconditional_arg( Kmap const& kmap
                          , Uuid const& cmd )
    -> bool;
[[ nodiscard ]]
auto parse_args( Kmap& kmap
               , Uuid const& cmd_id
               , std::string const& arg )
    -> Result< StringVec >;
[[ nodiscard ]]
auto fetch_params_ordered( Kmap& kmap
                         , Uuid const& cmd_id )
    -> Result< UuidVec >;
[[ nodiscard ]]
auto is_general_command( Kmap const& kmap
                       , Uuid const& id )
    -> bool;
[[ nodiscard ]]
auto is_particular_command( Kmap const& kmap
                          , Uuid const& id )
    -> bool;
[[ nodiscard ]]
auto to_command_name( Uuid const& id )
    -> std::string;

} // namespace kmap::cmd

#endif // KMAP_CMD_COMMAND_HPP
