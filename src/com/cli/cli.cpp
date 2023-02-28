/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"

#include "cli/parser.hpp"
#include "cmd.hpp"
#include "cmd/command.hpp"
#include "cmd/parser.hpp"
#include "cmd/select_node.hpp"
#include "com/canvas/canvas.hpp"
#include "com/network/network.hpp"
#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/order.hpp"
#include "util/result.hpp"

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

namespace kmap::com {

// TODO: Prereq: kmap is initialized.
auto register_commands()
    -> void
{
    using emscripten::val;
    using emscripten::vecFromJSArray;

    auto const cmds = vecFromJSArray< std::string >( val::global( "kmap_registration_commands" ) );

    for( auto const& cmd : cmds )
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

    auto const args = vecFromJSArray< std::string >( val::global( "kmap_registration_arguments" ) );

    for( auto const& arg : args )
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

auto Cli::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( install_events() );

    rv = outcome::success();

    return rv;
}

auto Cli::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( install_events() );

    rv = outcome::success();

    return rv;
}

auto Cli::install_events()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // onkeydown
    {
        auto const ctor =
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() ).onkeydown = function( e )
{{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_ctrl = !!e.ctrlKey;
    let is_shift = !!e.shiftKey;
    let text = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );

    const res = kmap.cli().on_key_down( key
                                        , is_ctrl
                                        , is_shift
                                        , text.value );

    if( key == 9/*tab*/
    || key == 13/*enter*/ )
    {{
        e.preventDefault();
    }}
    if( res.has_error() )
    {{
        console.log( 'CLI error: ' + res.error_message() );
    }}
}};)%%%";
        auto const dtor = 
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() ).onkeydown = null;)%%%";

        scoped_events_.emplace_back( ctor, dtor );
    }

    rv = outcome::success();

    return rv;
}

auto Cli::parse_raw( std::string const& input )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "input", input );

    auto rv = KMAP_MAKE_RESULT( std::string );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    if( auto const cbar = parser::cli::parse_command_bar( input )
      ; cbar )
    {
        // TODO: more complex CLI options probably warrant a "self propelled" state machine, but with just one opt sel and cmd, probably not worth it now.
        //       Actually, the driving events could be the "chain" of commands.c.
        auto const sel = parser::cli::fetch_selection_string( *cbar );
        auto const cmd = parser::cli::fetch_command_string( *cbar );

        if( sel && !cmd )
        {
            rv = cmd::select_node( km, *sel );
        }
        else if( sel && cmd )
        {
            auto const selected = nw->selected_node();

            if( rv = cmd::select_node( km, *sel )
              ; rv )
            {
                rv = execute( cmd->first, cmd->second );
            }

            // Return to original.
            if( nw->exists( selected ) ) // Corner case in which the command deletes the original node! TODO: Make test case for this.
            {
                KTRY( nw->select_node( selected ) );
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
        auto const nw = KTRY( fetch_component< com::VisualNetwork >() );

        nw->focus();
    }

    return rv;
}

auto Cli::execute( std::string const& cmd_str
                 , std::string const& arg )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "cmd", cmd_str );
        KM_RESULT_PUSH_STR( "arg", arg );
        
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto& km = kmap_inst();

    // This block temporary until old-style cmds are transitioned to new.
    if( auto const resolved_cmd = fetch_general_command_guard_resolved( cmd_str )
      ; resolved_cmd )
    {
        rv = KMAP_TRY( cmd::execute_command( km, resolved_cmd.value(), arg ) );

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
    auto const& km = kmap_inst();

    if( auto const desc = anchor::abs_root
                        | view2::direct_desc( "meta.setting.command" )
                        | view2::direct_desc( path )
                        | act2::fetch_node( km )
      ; desc )
    {
        rv = desc.value();
    }
    
    return rv;
}

auto Cli::resolve_contextual_guard( Uuid const& cmd ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "cmd", cmd );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const& km = kmap_inst();

    if( cmd::is_general_command( km, cmd ) )
    {
        for( auto const children = anchor::node( cmd )
                                 | view2::child
                                 | act2::to_node_set( km )
                                 | act::order( km )
           ; auto const guard : children )
        {
            if( cmd::evaluate_guard( km, guard, { "" } ) ) // Note: arg is omitted b/c this is the environmental guard, not argument guard.
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

    auto& km = kmap_inst();
    auto const nw = KTRYE( fetch_component< com::Network >() );

    if( auto const cbar = parser::cli::parse_command_bar( to_lower_copy( input ) )
      ; cbar )
    {
        // TODO: more complex CLI options probably warrant a "self propelled" state machine, but with just one opt sel and cmd, probably not worth it now.
        //       Actually, the driving events could be the "chain" of commands.c.
        auto const sel = parser::cli::fetch_selection_string( *cbar );
        auto const cmd = parser::cli::fetch_command_string( *cbar );

        if( sel && !cmd )
        {
            auto const completed_set = complete_selection( km
                                                         , km.root_node_id()
                                                         , nw->selected_node()
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
    auto const& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // BC_ASSERT( rv.size() >= input.size() );
        })
    ;

    auto const cmds_root = KTRYE( anchor::abs_root
                                | view2::direct_desc( "meta.setting.command" )
                                | act2::fetch_node( km ) );
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
    auto const completion_map = complete_path_reducing( km
                                                      , cmds_root
                                                      , cmds_root
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
    auto& km = kmap_inst();

    if( !args.empty() )
    {
        if( auto const cmd = fetch_general_command_guard_resolved( scmd )
          ; cmd )
        {
            if( auto const params = cmd::fetch_params_ordered( km, cmd.value() ) 
              ; params
             && params.value().size() >= args.size() )
            {
                auto const tcarg = args.back();
                auto const completer = KTRYE( anchor::node( params.value()[ args.size() - 1 ] )
                                            | view2::child( "completion" )
                                            | act2::fetch_node( km ) );
                auto const possible_completions = cmd::evaluate_completer( km
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

    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( canvas->cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    val::global( to_string( canvas->cli_pane() ).c_str() ).set( "value", out );
}

auto Cli::read()
    -> std::string
{
    using emscripten::val;
    using std::string;

    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( canvas->cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto const elem = val::global( to_string( canvas->cli_pane() ).c_str() )[ "value" ];

    assert( elem.as< bool >() );

    return elem.as< string >();
}

auto Cli::focus()
    -> void
{
    using emscripten::val;

    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( js::fetch_element_by_id< val >( to_string( canvas->cli_pane() ) ).has_value() );
        })
    ;

    auto elem = js::fetch_element_by_id< val >( to_string( canvas->cli_pane() ) );

    BC_ASSERT( elem );

    elem.value().call< val >( "focus" );
}

auto Cli::is_focused()
    -> bool
{
    using emscripten::val;

    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( canvas->cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto const elem = val::global( to_string( canvas->cli_pane() ).c_str() );
    auto const doc = val::global( "document" );

    return elem == doc[ "activeElement" ];
}

auto Cli::clear_input()
    -> void
{
    using emscripten::val;

    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( js::exists( canvas->cli_pane() ) );
        })
    ;

    auto elem = KTRYE( js::fetch_style_member( to_string( canvas->cli_pane() ) ) ); // TODO: KTRY?

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

    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( canvas->cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto elem = val::global( to_string( canvas->cli_pane() ).c_str() );

    elem.set( "readOnly", false );
}

auto Cli::disable_write()
    -> void
{
    using emscripten::val;

    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( canvas->cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto elem = val::global( to_string( canvas->cli_pane() ).c_str() );

    elem.set( "readOnly", true );
}

auto Cli::set_color( Color const& c )
    -> void
{
    using emscripten::val;

    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( !val::global( to_string( canvas->cli_pane() ).c_str() ).isUndefined() );
        })
    ;

    auto style = KTRYE( js::fetch_style_member( to_string( canvas->cli_pane() ) ) ); 

    style.set( "backgroundColor", to_string( c ) );
}

auto Cli::show_popup( std::string const& text )
    -> void
{
    using emscripten::val;

    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );
    auto const cli_dims = canvas->dimensions( canvas->cli_pane() ).value();
    auto box_dims = com::Dimensions{};
    auto elem = KMAP_TRYE( js::fetch_element_by_id< val >( to_string( canvas->completion_overlay() ) ) ); 
    auto style = KTRYE( js::fetch_style_member( to_string( canvas->completion_overlay() ) ) ); 
    auto const font_size = [ & ]
    {
        auto const t = KMAP_TRYE( js::fetch_computed_property_value< std::string >( to_string( canvas->completion_overlay() )
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
        auto const t = KMAP_TRYE( js::fetch_computed_property_value< std::string >( to_string( canvas->completion_overlay() )
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

        return lengths.empty() ? 0 : ranges::max( lengths );
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

    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    auto elem = KMAP_TRYE( js::fetch_element_by_id< val >( to_string( canvas->completion_overlay() ) ) ); 

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
    // update_pane();
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
}

auto Cli::on_key_down( int const key
                     , bool const is_ctrl
                     , bool const is_shift
                     , std::string const& text )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "key", std::to_string( key ) );
        KM_RESULT_PUSH_STR( "text", text );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( fetch_component< com::VisualNetwork >() );

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
                clear_input();
                nw->focus();
            }
            break;
        }
        case 67/*c*/:
        {
            if( is_ctrl )
            {
                clear_input();
                nw->focus();
            }
            break;
        }
    }

    rv = outcome::success();

    return rv;
}

auto Cli::parse_cli( std::string const& input )
    -> void // TODO: Quite sure this should be some kind of Result< >...
{
    KTRYE( parse_raw( input ) );
}

// TODO: Replace update panes with events
auto Cli::update_pane()
    -> void
{
    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    KTRYE( canvas->update_pane( canvas->cli_pane() ) ); // TODO: Find out why focusing requires a repositioning of the CLI element. That is, why calls to this are necessary to maintain dimensions.
}

namespace binding {

using namespace emscripten;

struct Cli
{
    kmap::Kmap& km;

    auto focus()
        -> void
    {
        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->focus();
    }
    auto on_key_down( int const key
                    , bool const is_ctrl
                    , bool const is_shift
                    , std::string const& text )
        -> kmap::binding::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRY( km.fetch_component< com::Cli >() );

        return cli->on_key_down( key
                               , is_ctrl 
                               , is_shift
                               , text );
    }
    auto notify_success( std::string const& message )
        -> void
    {
        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->notify_success( message );
    }
    auto notify_failure( std::string const& message )
        -> void
    {
        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->notify_failure( message );
    }
    auto parse_cli( std::string const& input )
        -> void
    {
        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->parse_cli( input );
    }
};

auto cli()
    -> com::binding::Cli
{
    return com::binding::Cli{ kmap::Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_cli )
{
    function( "cli", &kmap::com::binding::cli );
    class_< kmap::com::binding::Cli >( "Cli" )
        .function( "focus", &kmap::com::binding::Cli::focus )
        .function( "notify_failure", &kmap::com::binding::Cli::notify_failure )
        .function( "notify_success", &kmap::com::binding::Cli::notify_success )
        .function( "on_key_down", &kmap::com::binding::Cli::on_key_down )
        .function( "parse_cli", &kmap::com::binding::Cli::parse_cli );
        ;
}

} // namespace binding

namespace {
namespace cli_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Cli
,   std::set({ "canvas"s, "event_store"s, "visnetwork"s, "root_node"s })
,   "cli related functionality"
);

} // namespace cli_def 
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

} // namespace kmap::com
