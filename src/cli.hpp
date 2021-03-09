/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CLI_HPP
#define KMAP_CLI_HPP

#include "arg.hpp"
#include "common.hpp"
#include "network.hpp"
#include "utility.hpp"

#include <map>
#include <string>

namespace kmap
{

// TODO: Probably worth replacing with Boost.Outcome and "custom payloads" for adding additional contextual information about the failure. See Custom Payloads section in the doc.

// TODO:
// -needed: optional example list.
struct CliCommand
{
    using Arg = std::string;
    using Args = std::vector< std::string >;
    using Dispatch = std::function< Result< std::string >( Args const& ) >;

    std::string command = {};
    std::string help = {};
    ArgumentList args = {};
    Dispatch dispatch = {};
};

struct PreregisteredCommand
{
    struct Argument
    {
        Heading heading;
        std::string description;
        Heading argument_alias;
    };
    struct Guard
    {
        Heading heading;
        std::string code;
    };

    std::string path;
    std::string description;
    std::vector< Argument > arguments;
    Guard guard;
    std::string action;
};

struct PreregisteredArgument
{
    std::string path;
    std::string description;
    std::string guard;
    std::string completion;
};

class Kmap;

class Cli
{
public:
    using ValidCommandKeys = std::vector< std::string >;

    Cli( Kmap& kmap );

    auto parse_raw( std::string const& input )
        -> Result< std::string >;
    auto execute( std::string const& cmd
                , std::string const& arg )
        -> Result< std::string >;
    auto fetch_command( std::string const& cmd ) const
        -> Optional< CliCommand >;
    auto fetch_general_command( Heading const& path ) const
        -> Optional< Uuid >;
    auto resolve_contextual_guard( Uuid const& cmd ) const
        -> Result< Uuid >;
    auto fetch_general_command_guard_resolved( Heading const& path ) const
        -> Optional< Uuid >;
    auto complete( std::string const& input )
        -> void;
    auto complete_arg( Argument const& arg
                     , std::string const& input )
        -> StringVec;
    [[ nodiscard ]]
    auto complete_command( std::string const& input ) const
        -> StringVec;
    [[ nodiscard ]]
    auto complete_command( std::string const& cmd
                         , StringVec const& args )
        -> std::string;
    [[ nodiscard ]]
    auto create_argument( PreregisteredArgument const& arg )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto create_command( PreregisteredCommand const& prereg )
        -> Result< Uuid >;
    [[ nodiscard ]]
    auto on_key_down( int const key
                    , bool const is_ctrl
                    , bool const is_shift
                    , std::string const& text )
        -> Result< void >;
    auto write( std::string const& out )
        -> void;
    [[ nodiscard ]]
    auto read()
        -> std::string;
    auto focus()
        -> void;
    [[ nodiscard ]]
    auto is_focused()
        -> bool;
    auto clear_input()
        -> void;
    [[ nodiscard ]]
    auto valid_commands()
        -> std::vector< CliCommand >;
    auto enable_write()
        -> void;
    [[ nodiscard ]]
    auto evaluate_guard( Uuid const& guard_node )
        -> bool;
    auto disable_write()
        -> void;
    auto register_command( CliCommand const& cmd )
        -> void;
    auto register_command( PreregisteredCommand const& pregreg )
        -> void;
    auto register_argument( PreregisteredArgument const& arg )
        -> void;
    auto set_color( Color const& c )
        -> void;
    auto show_popup( std::string const& text )
        -> void;
    auto show_popup( StringVec const& lines )
        -> void;
    auto hide_popup()
        -> void;
    [[ nodiscard ]]
    auto get_input()
        -> std::string;
    [[ nodiscard ]]
    auto execute_command( Uuid const& id // TODO: I'm not entirely sure 'execute_command' belongs here. It's actually not specific to CLI (it could be used elsewhere, i.e., not originating from the CLI). Putting it here for now, as I'm not sure where else to put it.
                        , std::string const& code )
        -> bool;
    auto notify_success( std::string const& message )
        -> void;
    auto notify_failure( std::string const& message )
        -> void;
    auto reset_all_preregistered()
        -> Result< void >;
    auto update_pane()
        -> void;

protected:
    auto reset_preregistered_arguments()
        -> Result< void >;
    auto reset_preregistered_commands()
        -> Result< void >;

private:
    using CommandMap = std::map< std::string
                               , CliCommand >;

    Kmap& kmap_;
    CommandMap valid_cmds_ = {};
    std::vector< PreregisteredCommand > prereg_cmds_ = {};
    std::vector< PreregisteredArgument > prereg_args_ = {};
};

auto register_command( Kmap& kmap
                     , CliCommand const& cmd )
    -> bool;

} // namespace kmap

#endif // KMAP_CLI_HPP
