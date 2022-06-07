/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"

#include "canvas.hpp"
#include "cli/parser.hpp"
#include "cmd.hpp"
#include "cmd/command.hpp"
#include "cmd/parser.hpp"
#include "cmd/select_node.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/sml.hpp>
#include <emscripten.h>
#include <emscripten/val.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/max.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/drop_last.hpp>
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

namespace kmap {

auto reset_registrations( Kmap& kmap )
    -> void
{
    using emscripten::val;

    auto& cli = kmap.cli();

    if( auto const r = kmap.cli().reset_all_preregistered()
      ; !r )
    {
        KMAP_THROW_EXCEPTION_MSG( to_string( r.error() ) );
    }
    // Legacy commands... TODO: Remove all when legacy are transitioned to new method.
    for( auto const& c : cmd::make_core_commands( kmap ) )
    {
        cli.register_command( c );
    }
}

// TODO: Prereq: kmap is initialized.
auto register_commands()
    -> void
{
    using emscripten::val;
    using emscripten::vecFromJSArray;

    for( auto const& cmd : vecFromJSArray< std::string >( val::global( "registration_commands" ) ) )
    {
        if( auto const& succ = js::eval_void( fmt::format( "Module.register_cmd_{}();", cmd ) )
          ; !succ )
        {
            fmt::print( stderr
                      , "failed to register {}\n"
                      , cmd );
        }
    }
}

auto register_arguments()
    -> void
{
    using emscripten::val;
    using emscripten::vecFromJSArray;

    for( auto const& arg : vecFromJSArray< std::string >( val::global( "registration_arguments" ) ) )
    {
        if( auto const& succ = js::eval_void( fmt::format( "Module.register_arg_{}();", arg ) )
          ; !succ )
        {
            fmt::print( stderr
                      , "failed to register {}\n"
                      , arg );
        }
    }
}

Cli::Cli( Kmap& kmap )
    : kmap_{ kmap }
{
}

auto Cli::parse_raw( std::string const& input )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );

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
                KMAP_TRY( kmap_.select_node( selected ) );
            }
        }
        else if( cmd )
        {
            rv = execute( cmd->first, cmd->second );
        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "Syntax error" );
    }

    if( rv )
    {
        // Note: no longer need to notify of success, as this should be done automatically.
        // Only failure to find command or execute body should be reported.
        // A failure to execute the command is already handled.
        notify_success( rv.value() );
    }
    else
    {
        #if KMAP_DEBUG 
            io::print( stderr, "parse_raw() command failed: {}\n", to_string( rv.error() ) );
        #endif // KMAP_DEBUG

        BC_ASSERT( rv.has_error() );
        BC_ASSERT( !rv.error().stack.empty() );

        io::print( "{}\n", to_string( rv.error() ) );

        notify_failure( rv.error().stack.front().message );
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
    // TODO: Can I put a KMAP_ENSURE( cmd::parser::parse_command() ) here, to ensure the syntax is fine?
    prereg_cmds_.emplace_back( prereg );
}

auto Cli::register_argument( PreregisteredArgument const& arg )
    -> void 
{
    prereg_args_.emplace_back( arg );
}

auto Cli::execute( std::string const& cmd_str
                 , std::string const& arg )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
io::print( "execute.arg: {}\n", arg );

    // This block temporary until old-style cmds are transitioned to new.
    if( auto const resolved_cmd = fetch_general_command_guard_resolved( cmd_str )
      ; resolved_cmd )
    {
        rv = KMAP_TRY( cmd::execute_command( kmap_, resolved_cmd.value(), arg ) );

        return rv;
    }

    auto const cmd = fetch_command( cmd_str );
    auto const args = arg | views::split( ' ' ) | to< StringVec >();

    if( !cmd )
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "unrecognized command" );
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
            rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized
                                    , fmt::format( "expected between [{},{}] arguments, got {}"
                                                 , min_size
                                                 , max_size
                                                 , args.size() ) );
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
                rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized
                                        , fmt::format( "malformation at position: {}"
                                                     , *malformed ) );
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
    valid_cmds_.insert( { cmd.command, cmd } );
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
                                                              , "/meta.setting.command"
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
    io::print( "fetching: {}\n", path );

    if( auto const guarded = fetch_general_command( path )
      ; guarded )
    {
        io::print( "found: {}\n", path );
        if( auto const resolved = resolve_contextual_guard( guarded.value() ) )
        {
            io::print( "resolved: {}\n", path );
            rv = resolved.value();
        }
        else
        {
            io::print( "ctx guard failed: \n", to_string( resolved.error() ) );
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

    auto const cmds_root = kmap_.fetch_descendant( "/meta.setting.command" );
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
                auto const completer = KTRYE( view::make( params.value()[ args.size() - 1 ] )
                                            | view::child( "completion" )
                                            | view::fetch_node( kmap_ ) );
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
            BC_ASSERT( !val::global( to_string( kmap_.canvas().cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    val::global( to_string( kmap_.canvas().cli_pane() ).c_str() ).set( "value", out );
}

auto Cli::read()
    -> std::string
{
    using emscripten::val;
    using std::string;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( kmap_.canvas().cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto const elem = val::global( to_string( kmap_.canvas().cli_pane() ).c_str() )[ "value" ];

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
            BC_ASSERT( js::fetch_element_by_id< val >( to_string( kmap_.canvas().cli_pane() ) ).has_value() );
        })
    ;

    auto elem = js::fetch_element_by_id< val >( to_string( kmap_.canvas().cli_pane() ) );

    BC_ASSERT( elem );

    elem.value().call< val >( "focus" );

    update_pane();
}

auto Cli::is_focused()
    -> bool
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( kmap_.canvas().cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto const elem = val::global( to_string( kmap_.canvas().cli_pane() ).c_str() );
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
            BC_ASSERT( js::exists( kmap_.canvas().cli_pane() ) );
        })
    ;

    auto elem = val::global( to_string( kmap_.canvas().cli_pane() ).c_str() );

    elem.set( "value", "" );

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
            BC_ASSERT( !val::global( to_string( kmap_.canvas().cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto elem = val::global( to_string( kmap_.canvas().cli_pane() ).c_str() );

    elem.set( "readOnly", false );
}

auto Cli::disable_write()
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( kmap_.canvas().cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto elem = val::global( to_string( kmap_.canvas().cli_pane() ).c_str() );

    elem.set( "readOnly", true );
}

auto Cli::set_color( Color const& c )
    -> void
{
    using emscripten::val;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( kmap_.canvas().cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto elem = val::global( to_string( kmap_.canvas().cli_pane() ).c_str() );

    elem.set( "style"
            , fmt::format( "background-color: {}"
                         , to_string( c ) ) );
}

auto Cli::show_popup( std::string const& text )
    -> void
{
    using emscripten::val;

    auto& canvas = kmap_.canvas();
    auto const cli_dims = canvas.dimensions( canvas.cli_pane() ).value();
    auto box_dims = Dimensions{};
    auto elem = KMAP_TRYE( js::fetch_element_by_id< val >( to_string( canvas.completion_overlay() ) ) ); 
    auto style = KMAP_TRYE( js::eval< val >( io::format( "return document.getElementById( '{}' ).style;", to_string( canvas.completion_overlay() ) ) ) ); 
    auto const font_size = [ & ]
    {
        auto const t = KMAP_TRYE( js::fetch_computed_property_value< std::string >( to_string( canvas.completion_overlay() )
                                                                                  , "font-size" ) ); 

        if( t.ends_with( "px" ) )
        {
            return std::stoull( t | views::drop_last( 2 ) | to< std::string >() );
        }
        else
        {
            return std::stoull( t );
        }
    }();
    auto const padding = [ & ]
    {
        auto const t = KMAP_TRYE( js::fetch_computed_property_value< std::string >( to_string( canvas.completion_overlay() )
                                                                                  , "padding" ) ); 

        if( t.ends_with( "px" ) )
        {
            return std::stoull( t | views::drop_last( 2 ) | to< std::string >() );
        }
        else
        {
            return std::stoull( t );
        }
    }();
    auto const lines = [ & ]
    {
        auto ls = StringVec{};

        auto pos = size_t{ 0 };
        auto token = std::string{};
        auto const delimiter = std::string{ "<br>" };
        auto s = text;
        while( ( pos = s.find( delimiter ) ) != std::string::npos )
        {
            token = s.substr( 0, pos );
            s.erase( 0, pos + delimiter.length() );
            ls.emplace_back( token );
        }

        return ls;
    }();
    auto const line_count = lines.size() + 1;
    auto const max_width = [ & ]
    {
        auto const lengths = lines
                           | views::transform( []( auto const& e ){ return static_cast< uint32_t >( e.size() ); } ) // TODO: Checked conversion...
                           | to< std::vector< uint32_t > >();

        return ranges::max( lengths );
    }();

    box_dims.left = cli_dims.left;
    box_dims.right = padding + ( font_size * 0.5 * max_width ); // TODO: Figure out consistent way of determining text width.
    box_dims.bottom = cli_dims.top;
    box_dims.top = box_dims.bottom - ( ( font_size * line_count ) + ( ( font_size * line_count ) * .15 ) + padding ); // TODO: Figure out consistent way of determining text height. Presently, I'm hueristically determining height from "font-size".
    io::print( "dims: {}\n", cli_dims );

    BC_ASSERT( cli_dims.bottom >= cli_dims.top );
    BC_ASSERT( cli_dims.right >= cli_dims.left );

    style.set( "position", "absolute" );
    style.set( "top", io::format( "{}px", box_dims.top ) );
    style.set( "height", io::format( "{}px", box_dims.bottom - box_dims.top ) );
    style.set( "left", io::format( "{}px", box_dims.left ) );
    style.set( "width", io::format( "{}px", box_dims.right - box_dims.left ) );

    elem.set( "innerHTML", text );
    elem.set( "hidden", false );
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
    using emscripten::val;

    auto elem = KMAP_TRYE( js::fetch_element_by_id< val >( to_string( kmap_.canvas().completion_overlay() ) ) ); 

    elem.set( "hidden", true );
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
    auto const path = fmt::format(  "/setting.command.{}"
                                 , cmd.command );
    if( auto const cmd_root = kmap.fetch_or_create_leaf( path )
      ; cmd_root )
    {
        if( auto const args_root = kmap.fetch_or_create_leaf( *cmd_root
                                                            , "argument" )
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
    update_pane();
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
    write( fmt::format( "[failure] {}", message ) );
    disable_write();
    set_color( Color::red );
    update_pane();
}

auto Cli::create_argument( PreregisteredArgument const& arg )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    auto const arg_root = kmap_.fetch_or_create_descendant( kmap_.root_node_id()
                                                          , "/meta.setting.argument" ); // TODO: Should this be setting.command.argument instead?
    auto const arg_root_path = kmap_.absolute_path_flat( arg_root.value() );
    
    if( auto const existing = kmap_.fetch_descendant( io::format( "{}.{}", arg_root_path, arg.path ) )
      ; existing )
    {
        KMAP_TRY( kmap_.erase_node( existing.value() ) );
    } 

    auto const arglin = KMAP_TRY( kmap_.create_descendants( arg_root.value()
                                                          , io::format( "/{}", arg.path ) ) );
    auto const arg_node = arglin.back();
    auto const nodes = kmap_.create_children( arg_node
                                            , "description"
                                            , "guard"
                                            , "completion" );
    
    KMAP_TRY( kmap_.update_body( nodes[ 0 ], arg.description ) );
    KMAP_TRY( kmap_.update_body( nodes[ 1 ], arg.guard ) );
    KMAP_TRY( kmap_.update_body( nodes[ 2 ], arg.completion ) );

    rv = arg_node;

    return rv;
}

// TODO: This belongs in command.cpp, doesn't it?
auto Cli::create_command( PreregisteredCommand const& prereg )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const cmd_root = KMAP_TRY( kmap_.fetch_or_create_descendant( kmap_.root_node_id()
                                                                    , "/meta.setting.command" ) );
    auto const path_from_root = fmt::format( "/{}.{}"
                                           , prereg.path
                                           , prereg.guard.heading );

    if( auto const existing = kmap_.fetch_descendant( cmd_root, prereg.path )
      ; existing )
    {
        KMAP_TRY( kmap_.erase_node( existing.value() ) );
    } 

    auto const cmdlin = KMAP_TRY( kmap_.create_descendants( cmd_root, path_from_root ) );
    auto const cmd = cmdlin.back();
    auto const children = kmap_.create_children( cmd
                                               , "description"
                                               , "argument"
                                               , "action" );
    auto const vcmd = view::make( cmd );
    auto const descn = KTRY( vcmd | view::child( "description" ) | view::fetch_node( kmap_ ) );
    
    KMAP_TRY( kmap_.update_body( cmd, prereg.guard.code ) );
    KMAP_TRY( kmap_.update_body( descn, prereg.description ) );

    for( auto const& arg : prereg.arguments )
    {
        auto const argn = KTRY( vcmd | view::child( "argument" ) | view::fetch_node( kmap_ ) );
        auto const arg_src = KMAP_TRY( kmap_.fetch_descendant( fmt::format( "/meta.setting.argument.{}"
                                                                          , arg.argument_alias ) ) );
        auto const arg_dst = KMAP_TRY( kmap_.create_child( argn, arg.heading ) );

        KMAP_TRY( kmap_.update_body( arg_dst, arg.description ) );
        KMAP_TRY( kmap_.create_alias( arg_src, arg_dst ) );
    }

    auto const actn = KTRY( vcmd | view::child( "action" ) | view::fetch_node( kmap_ ) );
    KMAP_TRY( kmap_.update_body( actn, prereg.action ) );

    rv = cmd;

    return rv;
}

auto Cli::on_key_down( int const key
                     , bool const is_ctrl
                     , bool const is_shift
                     , std::string const& text )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    switch( key )
    {
        case 9/*tab*/:
        {
            complete( text );
            break;
        }
        case 13/*enter*/:
        {
            KMAP_TRY( parse_raw( read() ) );
            break;
        }
        case 27/*escape*/:
        {
            if( is_ctrl && 67 == key ) // ctrl+c
            {
                auto& nw = kmap_.network();
                
                clear_input();
                nw.focus();
            }
            break;
        }
        case 67/*c*/:
        {
            if( is_ctrl )
            {
                auto& nw = kmap_.network();
                
                clear_input();
                nw.focus();
            }
            break;
        }
    }

    rv = outcome::success();

    return rv;
}

auto Cli::reset_preregistered_arguments()
    -> Result< void > 
{
    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& prereg : prereg_args_ )
    {
        KMAP_TRY( create_argument( prereg ) );
    }

    rv = outcome::success();

    return rv;
}

auto Cli::reset_preregistered_commands()
    -> Result< void > 
{
    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& prereg : prereg_cmds_ )
    {
        KMAP_TRY( create_command( prereg ) );
    }

    rv = outcome::success();

    return rv;
}

auto Cli::reset_all_preregistered()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // Note: Must be applied the order below, as commands depends upon arguments.
    KMAP_TRY( reset_preregistered_arguments() );
    KMAP_TRY( reset_preregistered_commands() );

    rv = outcome::success();

    return rv;
}

auto Cli::update_pane()
    -> void
{
    kmap_.canvas().update_pane( kmap_.canvas().cli_pane() ).value(); // TODO: Find out why focusing requires a repositioning of the CLI element. That is, why to calls to this are necessary to maintain dimensions.
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
