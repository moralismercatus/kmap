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

#include <emscripten.h>
#include <emscripten/bind.h>

#if 0
#define REGISTER_COMMAND( path, desc, args, guard, action ) \
    namespace binding \
    { \
        auto KMAP_CONCAT( register_cmd_, __LINE__ )() \
        { \
            auto& kmap = Singleton::instance(); \
            return kmap.cli().register_command( { #path, desc, args, guard, action } ); \
        } \
        EMSCRIPTEN_BINDINGS( kmap_module_cmd ) \
        { \
            emscripten::function( format_heading( "register_cmd_"#path ).c_str(), &KMAP_CONCAT( register_cmd_, __LINE__ ) ); \
        } \
    }
#endif // 0

#define REGISTER_ARGUMENT( path, desc, guard, completion ) \
    namespace binding \
    { \
        auto KMAP_CONCAT( register_arg_, __LINE__ )() \
        { \
            auto& kmap = Singleton::instance(); \
            kmap.cli().register_argument( { #path, desc, guard, completion } ); \
        } \
        EMSCRIPTEN_BINDINGS( kmap_module_arg ) \
        { \
            emscripten::function( format_heading( "register_arg_"#path ).c_str(), &KMAP_CONCAT( register_arg_, __LINE__ ) ); \
        } \
    }

#include "com/cli/cli.hpp"
#include <functional>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

auto execute_command( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto execute_command( Kmap& kmap
                    , Uuid const& cmd_id
                    , std::string const& arg )
    -> Result< std::string >;
auto execute_kscript( Kmap& kmap 
                    , std::string const& code )
    -> Result< std::string >;
auto execute_javascript( Uuid const& node
                       , std::string_view const fn_body
                       , StringVec const& args )
    -> Result< std::string >;
auto execute_javascript( Uuid const& node
                       , std::string_view const fn_body
                       , StringVec const& arg )
    -> Result< std::string >;
[[ nodiscard ]]
auto evaluate_guard( Kmap const& kmap
                   , Uuid const& guard_node
                   , std::string const& arg )
    -> Result< std::string >;
[[ nodiscard ]]
auto evaluate_completer( Kmap& kmap
                       , Uuid const& completer_node
                       , std::string const& arg )
    -> Result< StringVec >;
[[ nodiscard ]]
auto has_unconditional_arg( Kmap const& kmap
                          , Uuid const& cmd
                          , Uuid const& args_root )
    -> bool;
[[ nodiscard ]]
auto fetch_args( Kmap& kmap
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
