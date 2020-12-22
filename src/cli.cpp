/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"

#include "cli/parser.hpp"
#include "cmd/command.hpp"
#include "cmd/parser.hpp"
#include "cmd/select_node.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/sml.hpp>
#include <emscripten.h>
#include <emscripten/val.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/intersperse.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>

#include <cctype>
#include <map>

using namespace ranges;
using namespace std::string_literals;

namespace kmap
{

CliCommandResult::operator bool() const
{
    return result == CliResultCode::success;
}

Cli::Cli( Kmap& kmap )
    : kmap_{ kmap }
{
}

auto Cli::parse_raw( std::string const& input )
    -> CliCommandResult
{
    auto rv = CliCommandResult{ CliResultCode::failure
                              , "Unknown failure" };

    if( auto const cbar = parser::cli::parse_command_bar( input )
      ; cbar )
    {
        // TODO: more complex CLI options probably warrant a "self propelled" state machine, but with just one opt sel and cmd, probably not worth it now.
        //       Actually, the driving events could be the "chain" of commands.c.
        auto const sel = parser::cli::fetch_selection_string( *cbar );
        auto const cmd = parser::cli::fetch_command_string( *cbar );

        if( sel && !cmd )
        {
            rv = cmd::select_node( kmap_, *sel );
        }
        else if( sel && cmd )
        {
            auto const selected = kmap_.selected_node();

            if( rv = cmd::select_node( kmap_, *sel )
              ; rv )
            {
                rv = execute( cmd->first, cmd->second );
            }

            // Return to original.
            if( kmap_.exists( selected ) ) // Corner case in which the command deletes the original node! TODO: Make test case for this.
            {
                kmap_.select_node( selected );
            }
        }
        else if( cmd )
        {
            rv = execute( cmd->first, cmd->second );
            if( !rv )
            {
                io::print( "execution failed: {}\n", rv.message );
            }
        }
    }
    else
    {
        // TODO: rv = ...syntax error details... from Spirit / parser.
        rv = CliCommandResult{ CliResultCode::failure
                             , "Syntax error" };
    }

    if( rv )
    {
        // Note: no longer need to notify of success, as this should be done automatically.
        // Only failure to find command or execute body should be reported.
        // A failure to execute the command is already handled.
        notify_success( rv.message );
    }
    else
    {
        notify_failure( rv.message );
    }

    if( is_focused() )
    {
        kmap_.network().focus();
    }
    
    return rv;
}

auto Cli::register_command( PreregisteredCommand const& prereg )
    -> void
{
    prereg_cmds_.emplace_back( prereg );
}

auto Cli::register_argument( PreregisteredArgument const& arg )
    -> void 
{
    prereg_args_.emplace_back( arg );
}

auto Cli::execute( std::string const& cmd_str
                 , std::string const& arg )
    -> CliCommandResult
{
    auto rv = CliCommandResult{ CliResultCode::failure
                              , "Unknown error" };

    // This block temporary until old-style cmds are transitioned to new.
    if( auto const resolved_cmd = fetch_general_command_guard_resolved( cmd_str )
      ; resolved_cmd )
    {
        rv = cmd::execute_command( kmap_, resolved_cmd.value(), arg );

        return rv;
    }

    auto const cmd = fetch_command( cmd_str );
    auto const args = arg | views::split( ' ' ) | to< StringVec >();

    if( !cmd )
    {
        rv = { CliResultCode::failure
             , "unrecognized command" };
    }
    else
    {
        auto const min_size = [ & ]
        {
            return count_if( cmd->args
                           , []( auto const& e )
            {
                return !e->is_optional();
            } );
        }();
        auto const max_size = [ & ]
        {
            if( cmd->args.size() == 1
             && dynamic_cast< TitleArg* >( &( *cmd->args[ 0 ] ) ) != nullptr ) // TODO: Find a better way. Hack to allow TitleArg accept an arbitrary number of arguments.
            {
                return std::numeric_limits< decltype( cmd->args.size() ) >::max();
            }
            else
            {
                return cmd->args.size();
            }
        }();

        if( args.size() < min_size
         || args.size() > max_size )
        {
            rv = { CliResultCode::failure
                 , fmt::format( "expected between [{},{}] arguments, got {}"
                              , min_size
                              , max_size
                              , args.size() ) };
        }
        else
        {
            auto const malformed = [ & ]() -> Optional< uint32_t >
            {
                for( auto i : views::indices( min( args.size()
                                                 , cmd->args.size() ) ) )
                {
                    if( auto const rv = cmd->args[ i ]
                                           ->is_fmt_malformed( args[ i ] )
                      ; rv )
                    {
                        return rv;
                    }
                }

                return nullopt;
            }();

            if( malformed )
            {
                rv = { CliResultCode::failure
                     , fmt::format( "malformation at position: {}"
                                  , *malformed ) };
            }
            else
            {
                rv = cmd->dispatch( args );
            }
        }
    }

    return rv;
}

auto Cli::register_command( CliCommand const& cmd )
    -> void
{
    valid_cmds_.insert( {cmd.command
                       , cmd} );
}

[[ deprecated( "used in old cmd style" ) ]]
auto Cli::fetch_command( std::string const& cmd ) const
    -> Optional< CliCommand >
{
    auto rv = Optional< CliCommand >{}; 
    auto const it = valid_cmds_.find( cmd );

    if( it != valid_cmds_.end() )
    {
        rv = it->second;
    }

    return rv;
}

// TODO: Probably belongs in command.cpp
auto Cli::fetch_general_command( Heading const& path ) const
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const desc =  kmap_.fetch_descendant( fmt::format( "{}.{}"
                                                              , "/meta.settings.commands"
                                                              , path ) )
      ; desc )
    {
        rv = desc.value();
    }
    
    return rv;
}

auto Cli::resolve_contextual_guard( Uuid const& cmd ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    if( cmd::is_general_command( kmap_, cmd ) )
    {
        for( auto const guard : kmap_.fetch_children_ordered( cmd ) )
        {
            if( cmd::evaluate_guard( kmap_, guard, { "" } ) ) // Note: arg is omitted b/c this is the environmental guard, not argument guard.
            {
                rv = guard;

                break;
            }
        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::command::not_general_command );
    }

    return rv;
}

// TODO: It'd be nice if this was easily composable: fetch_command | resolve_guard, rather than verbosely named.
auto Cli::fetch_general_command_guard_resolved( Heading const& path ) const
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const guarded = fetch_general_command( path )
      ; guarded )
    {
        if( auto const resolved = resolve_contextual_guard( guarded.value() ) )
        {
            rv = resolved.value();
        }
    }
    
    return rv;
}

auto Cli::complete( std::string const& input )
    -> void
{
    using boost::to_lower_copy;

    hide_popup(); // Clear previous popup.

    if( auto const cbar = parser::cli::parse_command_bar( to_lower_copy( input ) )
      ; cbar )
    {
        // TODO: more complex CLI options probably warrant a "self propelled" state machine, but with just one opt sel and cmd, probably not worth it now.
        //       Actually, the driving events could be the "chain" of commands.c.
        auto const sel = parser::cli::fetch_selection_string( *cbar );
        auto const cmd = parser::cli::fetch_command_string( *cbar );

        if( sel && !cmd )
        {
            auto const completed_set = complete_selection( kmap_
                                                         , kmap_.root_node_id()
                                                         , kmap_.selected_node()
                                                         , *sel );
            if( completed_set )
            {
                auto const completed = completed_set.value() | views::transform( &CompletionNode::path ) | to< StringSet >(); 

                if( completed.size() == 1 )
                {
                    auto const out = fmt::format( ":@{}"
                                                , *completed.begin() );
                    write( out );
                }
                else
                {
                    auto const completed_vec = completed | to< StringVec >();
                    auto const out = fmt::format( ":@{}"
                                                , longest_common_prefix( completed_vec ) );

                    // TODO: Before showing popup, eliminate common headings to shorten the hints to just the important parts.
                    show_popup( completed_vec );
                    write( out );
                }
            }
        }
        else if( sel && cmd )
        {
            auto const ccmd = complete_command( cmd->first
                                              , cmd->second | views::split( ' ' ) | to< StringVec >() );
            auto const out = fmt::format( ":@{} :{}"
                                        , *sel
                                        , ccmd );

            write( out );
        }
        else if( cmd )
        {
            auto const ccmd = complete_command( cmd->first
                                              , cmd->second | views::split( ' ' ) | to< StringVec >() );
            auto const out = fmt::format( ":{}"
                                        , ccmd );

            write( out );
        }
    }

    return;
}

/// An empty result means no completion found.
// TODO: This could be a free function.
auto Cli::complete_arg( Argument const& arg
                      , std::string const& input )
    -> StringVec
{
    auto rv = StringVec{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv ) { BC_ASSERT( e.size() >= input.size() ); }
        })
    ;

    rv = arg.complete( input );

    return rv;
}

auto Cli::complete_command( std::string const& input ) const
    -> StringVec
{
    auto rv = StringVec{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // BC_ASSERT( rv.size() >= input.size() );
        })
    ;

    auto const cmds_root = kmap_.fetch_descendant( "/meta.settings.commands" );
    auto const ctx_filter = views::filter( [ & ]( auto const& e )
    {
        auto const resolved = resolve_contextual_guard( e.target );

        if( !resolved )
        {
            io::print( "unresolved: {}\n", resolved.error().ec.message() );
        }

        if( resolved )
        {
            return true;
        }
        else if( resolved.error().ec == error_code::command::not_general_command )
        {
            return true;
        }
        else
        {
            return false;
        }
    } );
    auto const completion_map = complete_path_reducing( kmap_
                                                      , cmds_root.value()
                                                      , cmds_root.value()
                                                      , input );
    if( completion_map )
    {
        auto const possible_completions = completion_map.value()
                                        | ctx_filter
                                        | views::transform( &CompletionNode::path )
                                        | to< StringVec >();
        
        rv = possible_completions;
    }

    return rv;
}

auto Cli::complete_command( std::string const& scmd
                          , StringVec const& args )
    -> std::string
{
    auto rv = fmt::format( "{} {}"
                         , scmd
                         , flatten( args ) );

    if( !args.empty() )
    {
        if( auto const cmd = fetch_general_command_guard_resolved( scmd )
          ; cmd )
        {
            if( auto const params = cmd::fetch_params_ordered( kmap_, cmd.value() ) 
              ; params
             && params.value().size() >= args.size() )
            {
                auto const tcarg = args.back();
                auto const completer = kmap_.node_view( params.value()[ args.size() - 1 ] )[ "/completion" ];
                auto const possible_completions = cmd::evaluate_completer( kmap_
                                                                         , completer
                                                                         , tcarg );

                if( possible_completions )
                {
                    if( possible_completions.value().size() == 1 )
                    {
                        hide_popup();

                        rv = fmt::format( "{} {}"
                                        , scmd
                                        , possible_completions.value()[ 0 ] );
                    }
                    else
                    {
                        show_popup( possible_completions.value() );

                        rv = fmt::format( "{} {}"
                                        , scmd
                                        , longest_common_prefix( possible_completions.value() ) );
                    }
                }
                else
                {
                    show_popup( possible_completions.error().ec.message() );
                }
            }
        }
    }
    else
    {
        auto const completions = complete_command( scmd );

        if( completions.empty() )
        {
            rv = scmd;
        }
        if( completions.size() == 1 )
        {
            hide_popup();

            rv = completions[ 0 ];
        }
        else
        {
            show_popup( completions );

            rv = longest_common_prefix( completions ); 
        }
    }

    return rv;
}

auto Cli::write( std::string const& out )
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    val::global( "cli_input" ).set( "value"
                                  , out );
}

auto Cli::read()
    -> std::string
{
    using emscripten::val;
    using std::string;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    auto const elem = val::global( "cli_input" )[ "value" ];

    assert( elem.as< bool >() );

    return elem.as< string >();
}

auto Cli::focus()
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    auto elem = val::global( "cli_input" );

    elem.call< val >( "focus" );
}

auto Cli::is_focused()
    -> bool
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    auto const elem = val::global( "cli_input" );
    auto const doc = val::global( "document" );

    return elem == doc[ "activeElement" ];
}

auto Cli::clear_input()
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    auto elem = val::global( "cli_input" );

    elem.set( "value"
            , "" );

    hide_popup();
}

auto Cli::valid_commands()
    -> std::vector< CliCommand >
{
    return valid_cmds_ | views::values
                       | to_vector;
}

auto Cli::enable_write()
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    auto elem = val::global( "cli_input" );

    elem.set( "readOnly"
            , false );
}

auto Cli::disable_write()
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    auto elem = val::global( "cli_input" );

    elem.set( "readOnly"
            , true );
}

auto Cli::set_color( Color const& c )
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_input" ).isUndefined() );
        })
    ;

    auto elem = val::global( "cli_input" );

    elem.set( "style"
            , fmt::format( "background-color: {}"
                         , to_string( c ) ) );
}

auto Cli::show_popup( std::string const& text )
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "cli_popup" ).isUndefined() );
        })
    ;

    auto elem = val::global( "cli_popup" );

    elem.set( "innerHTML"
            , text );

    // TODO: how to call elem.classList.call< val >( "add", "show" )?
    EM_ASM(
    {
        let popup = document.getElementById( "cli_popup" );

        popup.classList
             .add( "show" );
    }
    , text.c_str() ); // TODO: Is this line dead code?
}

auto Cli::show_popup( StringVec const& lines )
    -> void
{
    auto formatted = lines
                   | views::intersperse( std::string{ "<br>" } )
                   | to_vector;

    show_popup( formatted
              | views::join
              | to< std::string >() );
}

auto Cli::hide_popup()
    -> void
{
    // TODO: how to call elem.classList.call< val >( "remove", "show" )?
    EM_ASM(
    {
        let popup = document.getElementById( "cli_popup" );

        popup.classList
             .remove( "show" );
    } );
}

/// Assumes any completion/abortion of input will unfocus CLI.
auto Cli::get_input()
    -> std::string
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( is_focused() );
        })
    ;

    assert( false );
    // while( is_focused() ) // This just deadlocks the program... doesn't work
    // at all... need to simple remove this routine.
    // {
    // }

    return read();
}

auto Cli::execute_command( Uuid const& id
                         , std::string const& code )
    -> bool
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( "append_script" ).isUndefined() );
        })
    ;

    auto const sid = to_string( id );
    auto const cid = sid
                   | views::remove( '-' )
                   | to< std::string >();
    auto const fn = fmt::format( "cmd_{}"
                               , cid );
    
    // TODO: The registration "append_script call" should happen at the callback of writes to body (or leaving body editor).
    //       This function should merely translate the id to a cmd_* function and call it.

    auto append_script = val::global( "append_script" );

    append_script( fn
                 , code );

    fmt::print( "Command {} updated\n", fn );

    return true;
}

auto register_command( Kmap& kmap
                     , CliCommand const& cmd )
    -> bool 
{
    auto rv = bool{};
    #if 0
    auto const path = fmt::format(  "/settings.commands.{}"
                                 , cmd.command );
    if( auto const cmd_root = kmap.fetch_or_create_leaf( path )
      ; cmd_root )
    {
        if( auto const args_root = kmap.fetch_or_create_leaf( *cmd_root
                                                            , "arguments" )
          ; args_root )
        {
            for( auto const& arg : cmd.args )
            {
                kmap.create_alias( arg
                                 , root );
            }

            return true;
        }
    }

    if( !rv )
    {
        kmap.delete_node( path );
    }
    #endif // 0

    return rv;
}

auto Cli::notify_success( std::string const& message )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    clear_input();
    write( fmt::format( "[success] {}"
                      , message ) );
    disable_write();
    set_color( Color::green );
}

auto Cli::notify_failure( std::string const& message )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    clear_input();
    write( fmt::format( "[failure] {}"
                      , message ) );
    disable_write();
    set_color( Color::red );
}

auto Cli::create_argument( PreregisteredArgument const& arg )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    auto const arg_root = kmap_.fetch_or_create_descendant( kmap_.root_node_id()
                                                          , "/meta.settings.arguments" ); // TODO: Should this be setting.command.argument instead?
    auto const arg_root_path = kmap_.absolute_path_flat( arg_root.value() );
    
    if( auto const existing = kmap_.fetch_descendant( io::format( "{}.{}", arg_root_path, arg.path ) )
      ; existing )
    {
        KMAP_TRY( kmap_.delete_node( existing.value() ) );
    } 

    auto const arglin = KMAP_TRY( kmap_.create_descendants( arg_root.value()
                                                          , io::format( "/{}", arg.path ) ) );
    auto const arg_node = arglin.back();
    auto const nodes = kmap_.create_children( arg_node
                                            , "description"
                                            , "guard"
                                            , "completion" );
    
    kmap_.update_body( nodes[ 0 ], arg.description );
    kmap_.update_body( nodes[ 1 ], arg.guard );
    kmap_.update_body( nodes[ 2 ], arg.completion );

    rv = arg_node;

    return rv;
}

// TODO: This belongs in command.cpp, doesn't it?
auto Cli::create_command( PreregisteredCommand const& prereg )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const cmd_root = KMAP_TRY( kmap_.fetch_or_create_descendant( kmap_.root_node_id()
                                                                    , "/meta.settings.commands" ) );
    auto const path_from_root = fmt::format( "/{}.{}"
                                           , prereg.path
                                           , prereg.guard.heading );

    if( auto const existing = kmap_.fetch_descendant( cmd_root, prereg.path )
      ; existing )
    {
        KMAP_TRY( kmap_.delete_node( existing.value() ) );
    } 

    auto const cmdlin = KMAP_TRY( kmap_.create_descendants( cmd_root
                                                          , path_from_root ) );
    auto const cmd = cmdlin.back();
    auto const children = kmap_.create_children( cmd
                                               , "description"
                                               , "arguments"
                                               , "action" );
    auto const nodes = kmap_.node_view( cmd );
    
    kmap_.update_body( cmd
                     , prereg.guard.code );
    kmap_.update_body( nodes[ "/description" ]
                     , prereg.description );
    for( auto const& arg : prereg.arguments )
    {
        auto const arg_src = KMAP_TRY( kmap_.fetch_descendant( fmt::format( "/meta.settings.arguments.{}"
                                                                          , arg.argument_alias ) ) );
        auto const arg_dst = KMAP_TRY( kmap_.create_child( nodes[ "/arguments" ], arg.heading ) );

        kmap_.update_body( arg_dst
                         , arg.description );

        KMAP_TRY( kmap_.create_alias( arg_src, arg_dst ) );
    }
    kmap_.update_body( nodes[ "/action" ]
                     , prereg.action );

    rv = cmd;

    return rv;
}

auto Cli::reset_preregistered_arguments()
    -> bool 
{
    bool rv = true; // TODO: Propagate notification that a command failed to be registered.

    for( auto const& prereg : prereg_args_ )
    {
        auto const res = create_argument( prereg );

        if( !res )
        {
            io::print( stderr
                     , "failed to create argument: {}: {}\n"
                     , prereg.path
                     , to_string( res.error() ) );
            // TODO: propagate out.
        }

        rv = res.has_value();
    }

    return rv;
}

auto Cli::reset_preregistered_commands()
    -> bool 
{
    bool rv = true; // TODO: Propagate notification that a command failed to be registered.

    for( auto const& prereg : prereg_cmds_ )
    {
        auto const res = create_command( prereg );

        if( !res )
        {
            io::print( stderr
                     , "failed to create command: {}: {}\n"
                     , prereg.path
                     , to_string( res.error() ) );
            // TODO: propagate out.
        }

        rv = res.has_value();
    }

    return rv;
}

auto Cli::reset_all_preregistered()
    -> bool
{
    // Note: Must be applied the order below, as commands depends upon arguments.
    return reset_preregistered_arguments()
        && reset_preregistered_commands();
}

/* :<cmd> args ...
  /<regex>
  @<path>
  @<path> :<cmd>
  @<path> /opts/regex

  Start -> Search [ push_rest ]
  Start -> Command [ push_cmd ]
  Start -> Select [ push_rest ]

  Command -> Args

  Args -> Arg Arg
  Args -> Arg

  Select -> Command
  Select -> Search
 */

} // namespace kmap
