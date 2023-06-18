/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CLI_HPP
#define KMAP_CLI_HPP

#include "arg.hpp"
#include "com/network/visnetwork.hpp"
#include "common.hpp"
#include "component.hpp"
#include "js_iface.hpp"
#include "utility.hpp"
#include <com/canvas/pane_clerk.hpp>
#include <com/event/event_clerk.hpp>
#include <com/option/option_clerk.hpp>

#include <map>
#include <string>

namespace kmap
{
    class Kmap;
}

namespace kmap::com {


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

class Cli : public Component
{
    using ValidCommandKeys = std::vector< std::string >;
    using CommandMap = std::map< std::string
                               , CliCommand >;

    CommandMap valid_cmds_ = {};
    OptionClerk oclerk_;
    EventClerk eclerk_;
    PaneClerk pclerk_;
    std::vector< js::ScopedCode > scoped_events_ = {};

public:
    static constexpr auto id = "cli";
    constexpr auto name() const -> std::string_view override { return id; }

    Cli( Kmap& km
       , std::set< std::string > const& requisites
       , std::string const& description );
    virtual ~Cli() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;

    auto register_panes()
        -> Result< void >;
    auto register_standard_options()
        -> Result< void >;
    auto register_standard_outlets()
        -> void;

    auto clear_input()
        -> Result< void >;
    auto complete( std::string const& input )
        -> void;
    auto complete_arg( kmap::Argument const& arg
                     , std::string const& input )
        -> StringVec;
    [[ nodiscard ]]
    auto complete_command( std::string const& input ) const
        -> StringVec;
    [[ nodiscard ]]
    auto complete_command( std::string const& cmd
                         , StringVec const& args )
        -> std::string;
    auto disable_write()
        -> void;
    auto enable_write()
        -> void;
    [[ nodiscard ]]
    auto evaluate_guard( Uuid const& guard_node )
        -> bool;
    auto execute( std::string const& cmd
                , std::string const& arg )
        -> Result< void >;
    [[ nodiscard ]]
    auto execute_command( Uuid const& id // TODO: I'm not entirely sure 'execute_command' belongs here. It's actually not specific to CLI (it could be used elsewhere, i.e., not originating from the CLI). Putting it here for now, as I'm not sure where else to put it.
                        , std::string const& code )
        -> bool;
    auto fetch_command( std::string const& cmd ) const
        -> Result< CliCommand >;
    auto fetch_general_command( Heading const& path ) const
        -> Result< Uuid >;
    auto fetch_general_command_guard_resolved( Heading const& path ) const
        -> Result< Uuid >;
    auto focus()
        -> void;
    [[ nodiscard ]]
    auto get_input()
        -> std::string;
    // TODO: Should be an event-based outlet.
    auto hide_popup()
        -> Result< void >;
    auto install_events()
        -> Result< void >;
    [[ nodiscard ]]
    auto is_focused()
        -> bool;
    [[ nodiscard ]]
    auto on_key_down( int const key
                    , bool const is_ctrl
                    , bool const is_shift
                    , std::string const& text )
        -> Result< void >;
    auto parse_cli( std::string const& input )
        -> void; // Return simple void to simplify EMCC binding.
    auto parse_raw( std::string const& input )
        -> Result< void >;
    [[ nodiscard ]]
    auto read()
        -> std::string;
    auto resolve_contextual_guard( Uuid const& cmd ) const
        -> Result< Uuid >;
    auto set_color( Color const& c )
        -> void;
    auto show_popup( std::string const& text )
        -> Result< void >;
    auto show_popup( StringVec const& lines )
        -> Result< void >;
    auto notify_success( std::string const& message )
        -> Result< void >;
    auto notify_failure( std::string const& message )
        -> Result< void >;
    auto update_pane()
        -> void;
    [[ nodiscard ]]
    auto valid_commands()
        -> std::vector< CliCommand >;
    auto write( std::string const& out )
        -> void;

protected:
    auto parse_raw_internal( std::string const& input )
        -> Result< void >;
};

} // namespace kmap::com

#endif // KMAP_CLI_HPP
