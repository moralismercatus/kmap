/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"

#include "cli/parser.hpp"
#include "cmd.hpp"
#include "cmd/command.hpp"
#include <com/cmd/command.hpp>
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

Cli::Cli( Kmap& km
        , std::set< std::string > const& requisites
        , std::string const& description )
    : Component( km, requisites, description )
    , oclerk_{ km }
    , eclerk_{ km, { Cli::id } }
    , pclerk_{ km }
{
    KM_RESULT_PROLOG();

    KTRYE( register_panes() );
    KTRYE( register_standard_options() );
    register_standard_outlets();
}

auto Cli::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( pclerk_.install_registered() );
    KTRY( oclerk_.install_registered() );
    KTRY( eclerk_.install_registered() );
    KTRY( install_events() );

    rv = outcome::success();

    return rv;
}

auto Cli::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( pclerk_.check_registered() );
    KTRY( oclerk_.check_registered() );
    KTRY( eclerk_.check_registered() );
    KTRY( install_events() );

    rv = outcome::success();

    return rv;
}

auto Cli::register_panes()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( pclerk_.register_pane( Pane{ .id = cli_uuid
                                     , .heading = "cli"
                                     , .division = Division{ Orientation::horizontal, 0.975f, false, "input" } } ) );
    KTRY( pclerk_.register_overlay( Overlay{ .id = completion_overlay_uuid 
                                           , .heading = "cli_completion"
                                           , .hidden = true
                                           , .elem_type = "div" } ) );

    rv = outcome::success();

    return rv;
}

auto Cli::register_standard_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    // CLI box
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.cli.background.color"
                                         , .descr = "Sets the background color for the cli pane."
                                         , .value = "\"#222222\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ) ).style.backgroundColor = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.cli.text.color"
                                         , .descr ="Sets the text color for the cli pane."
                                         , .value = "\"white\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ) ).style.color = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.cli.text.size"
                                         , .descr = "Text size."
                                         , .value = "\"large\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ) ).style.fontSize = option_value;" } ) );
    // Completion Box
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.background.color"
                                         , .descr = "Sets the background color for the completion box."
                                         , .value = "\"#333333\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.backgroundColor = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.text.color"
                                         , .descr = "Sets the text color for the completion box popup."
                                         , .value = "\"white\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.color = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.padding"
                                         , .descr = "Sets the padding between edge of box and internal text."
                                         , .value = "\"0px\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.padding = option_value;" } ) );
    KTRY( oclerk_.register_option( Option { .heading = "canvas.completion_box.border.radius"
                                          , .descr = "Sets the rounding radius for the corners of the box."
                                          , .value = "\"5px\""
                                          , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.borderRadius = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.border.style"
                                         , .descr = "Border style"
                                         , .value = "\"outset\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.borderStyle = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.border.width"
                                         , .descr = "Width of border."
                                         , .value = "\"thin\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.borderWidth = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.scrollbar"
                                         , .descr = "Specify scroll behavior."
                                         , .value = "\"auto\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.overflow = option_value;" } ) );
    // I think all canvas items are absolute... I think this gets encoded when they are created. Probably doesn't belong here.
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.position_type"
                                         , .descr = "Sets the rounding radius for the corners of the box."
                                         , .value = "\"absolute\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.position = option_value;" } ) );
    KTRY( oclerk_.register_option( Option{ .heading = "canvas.completion_box.placement_order"
                                         , .descr = "Specifies order in which box will be placed vis-a-vis other canvas elements."
                                         , .value = "\"9999\""
                                         , .action = "document.getElementById( kmap.uuid_to_string( kmap.canvas().completion_overlay() ) ).style.zIndex = option_value;" } ) );
    
    rv = outcome::success();

    return rv;
}

auto Cli::register_standard_outlets()
    -> void
{
    // branch
    {
        eclerk_.register_outlet( Branch{ .heading = "cli.key.shift.depressed"
                                       , .requisites = { "subject.window", "verb.depressed", "object.keyboard.key.shift" } } );
    }
    // enter_command
    {
        auto const action =
R"%%%(
const cli = kmap.cli();

cli.clear_input();
cli.focus();
cli.write( ':' );
cli.enable_write();
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "cli.key.shift.depressed.enter_command"
                                     , .requisites = { "subject.window", "verb.depressed", "object.keyboard.key.colon" }
                                     , .description = "opens CLI for command entry"
                                     , .action = action } );
    }
    // enter_heading
    {
        auto const action =
R"%%%(
const cli = kmap.cli();

cli.clear_input();
cli.focus();
cli.write( ':@' );
cli.enable_write();
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "cli.key.shift.depressed.enter_heading"
                                     , .requisites = { "subject.window", "verb.depressed", "object.keyboard.key.atsym" }
                                     , .description = "opens CLI for heading entry"
                                     , .action = action } );
    }
    // enter_search
    {
        auto const action =
R"%%%(
const cli = kmap.cli();

cli.clear_input();
cli.focus();
cli.write( ':/' );
cli.enable_write();
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "cli.enter_search"
                                     , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.fslash" }
                                     , .description = "opens CLI for search"
                                     , .action = action } );
    }
}

auto Cli::install_events()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // onkeydown
    {
        auto const ctor =
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ) ).onkeydown = function( e )
{{
    let key = e.keyCode ? e.keyCode : e.which;
    let is_ctrl = !!e.ctrlKey;
    let is_shift = !!e.shiftKey;
    let text = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ) );

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
        console.error( 'CLI error: ' + res.error_message() );
    }}
}};)%%%";
        auto const dtor = 
R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ) ).onkeydown = null;)%%%";

        scoped_events_.emplace_back( ctor, dtor );
    }

    rv = outcome::success();

    return rv;
}

auto Cli::parse_raw( std::string const& input )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "input", input );

    auto rv = result::make_result< void >();
    auto const pres = parse_raw_internal( input );

    if( pres )
    {
        KTRY( notify_success( input ) );

        rv = outcome::success();
    }
    else
    {
        #if KMAP_DEBUG 
            io::print( stderr, "parse_raw() command failed: {}\n", to_string( rv.error() ) );
        #endif // KMAP_DEBUG

        BC_ASSERT( pres.has_error() );
        BC_ASSERT( !pres.error().stack.empty() );

        KTRY( notify_failure( pres.error().stack.front().message ) );

        rv = KMAP_PROPAGATE_FAILURE( pres );
    }

    if( is_focused() )
    {
        auto const nw = KTRY( fetch_component< com::VisualNetwork >() );

        nw->focus();
    }

    return rv;
}

auto Cli::parse_raw_internal( std::string const& input )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "input", input );

    auto rv = result::make_result< void >();
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    auto const cbar = KTRY( parser::cli::parse_command_bar( input ) );
    // TODO: more complex CLI options probably warrant a "self propelled" state machine, but with just one opt sel and cmd, probably not worth it now.
    //       Actually, the driving events could be the "chain" of commands.c.
    auto const sel = parser::cli::fetch_selection_string( cbar );
    auto const cmd = parser::cli::fetch_command_string( cbar );

    if( sel && !cmd )
    {
        KTRY( cmd::select_node( km, sel.value() ) );
    }
    else if( sel && cmd )
    {
        auto const selected = nw->selected_node();

        KTRY( cmd::select_node( km, sel.value() ) );
        KTRY( execute( cmd->first, cmd->second ) );

        // Return to original.
        if( nw->exists( selected ) ) // Corner case in which the command deletes the original node! TODO: Make test case for this.
        {
            KTRY( nw->select_node( selected ) );
        }
    }
    else if( cmd )
    {
        KTRY( execute( cmd->first, cmd->second ) );
    }

    rv = outcome::success();

    return rv;
}

auto Cli::execute( std::string const& cmd_str
                 , std::string const& arg )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "cmd", cmd_str );
        KM_RESULT_PUSH_STR( "arg", arg );
        
    auto rv = result::make_result< void >();
    auto& km = kmap_inst();

    // This block temporary until old-style cmds are transitioned to new.
    auto const resolved_cmd = KTRY( fetch_general_command_guard_resolved( cmd_str ) );

    KTRY( cmd::execute_command( km, resolved_cmd, arg ) );

    rv = outcome::success();

    return rv;
}

[[ deprecated( "used in old cmd style" ) ]]
auto Cli::fetch_command( std::string const& cmd ) const
    -> Result< CliCommand >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< CliCommand >(); 
    auto const it = valid_cmds_.find( cmd );

    if( it != valid_cmds_.end() )
    {
        rv = it->second;
    }

    return rv;
}

// TODO: Probably belongs in command.cpp
auto Cli::fetch_general_command( Heading const& path ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path );

    auto rv = result::make_result< Uuid >();
    auto const& km = kmap_inst();

    rv = KTRY( view2::cmd::command_root
             | view2::direct_desc( path )
             | act2::fetch_node( km ) );
    
    return rv;
}

auto Cli::resolve_contextual_guard( Uuid const& cmd ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "cmd", cmd );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const& km = kmap_inst();

    KMAP_ENSURE( anchor::node( cmd ) | view2::cmd::command | act2::exists( km ), error_code::command::not_general_command );
    // KMAP_ENSURE( cmd::is_general_command( km, cmd ), error_code::command::not_general_command );

    for( auto const children = anchor::node( cmd )
                             | view2::child( "guard" )
                             | view2::child // TODO: `child( view2::cmd::guard )` - where view2::cmd::guard is a predicate that ensures that the node is a valid "guard" node/alias.
                             | view2::order
                             | act2::to_node_vec( km )
        ; auto const guard : children )
    {
        if( cmd::evaluate_guard( km, guard, { "" } ) ) // Note: arg is omitted b/c this is the environmental guard, not argument guard.
        {
            rv = cmd;

            break;
        }
    }

    return rv;
}

// TODO: It'd be nice if this was easily composable: fetch_command | resolve_guard, rather than verbosely named.
auto Cli::fetch_general_command_guard_resolved( Heading const& path ) const
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path );

    auto rv = result::make_result< Uuid >();
    io::print( "fetching: {}\n", path );

    auto const guarded = KTRY( fetch_general_command( path ) );
    io::print( "found: {}\n", path );

    rv = KTRY( resolve_contextual_guard( guarded ) );
    
    return rv;
}

auto Cli::complete( std::string const& input )
    -> void
{
    using boost::to_lower_copy;

    KM_RESULT_PROLOG();

    KTRYE( hide_popup() ); // Clear previous popup.

    auto& km = kmap_inst();
    auto const nw = KTRYE( fetch_component< com::Network >() );

    if( auto const cbar = parser::cli::parse_command_bar( to_lower_copy( input ) )
      ; cbar )
    {
        // TODO: more complex CLI options probably warrant a "self propelled" state machine, but with just one opt sel and cmd, probably not worth it now.
        //       Actually, the driving events could be the "chain" of commands.c.
        auto const sel = parser::cli::fetch_selection_string( cbar.value() );
        auto const cmd = parser::cli::fetch_command_string( cbar.value() );

        if( sel && !cmd )
        {
            auto const completed_set = complete_selection( km
                                                         , km.root_node_id()
                                                         , nw->selected_node()
                                                         , sel.value() );
            if( completed_set )
            {
                auto const completed = completed_set.value() | views::transform( &CompletionNode::path ) | to< StringSet >(); 

                if( completed.size() == 1 )
                {
                    auto const out = fmt::format( ":@{}", *completed.begin() );
                    write( out );
                }
                else
                {
                    auto const completed_vec = completed | to< StringVec >();
                    auto const out = fmt::format( ":@{}", longest_common_prefix( completed_vec ) );

                    // TODO: Before showing popup, eliminate common headings to shorten the hints to just the important parts.
                    KTRYE( show_popup( completed_vec ) );
                    write( out );
                }
            }
        }
        else if( sel && cmd )
        {
            auto const ccmd = complete_command( cmd->first
                                              , cmd->second | views::split( ' ' ) | to< StringVec >() );
            auto const out = fmt::format( ":@{} :{}"
                                        , sel.value()
                                        , ccmd );

            write( out );
        }
        else if( cmd )
        {
            auto const ccmd = complete_command( cmd->first
                                              , cmd->second | views::split( ' ' ) | to< StringVec >() );
            auto const out = fmt::format( ":{}", ccmd );

            write( out );
        }
    }

    return;
}

/// An empty result means no completion found.
// TODO: This could be a free function.
auto Cli::complete_arg( kmap::Argument const& arg
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
    KM_RESULT_PROLOG();

    auto rv = StringVec{};
    auto const& km = kmap_inst();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // BC_ASSERT( rv.size() >= input.size() );
        })
    ;

    auto const cmds_root = KTRYE( view2::cmd::command_root
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
    KM_RESULT_PROLOG();

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
                        KTRYE( hide_popup() );

                        rv = fmt::format( "{} {}"
                                        , scmd
                                        , possible_completions.value()[ 0 ] );
                    }
                    else
                    {
                        KTRYE( show_popup( possible_completions.value() ) );

                        rv = fmt::format( "{} {}"
                                        , scmd
                                        , longest_common_prefix( possible_completions.value() ) );
                    }
                }
                else
                {
                    KTRYE( show_popup( possible_completions.error().ec.message() ) );
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
            KTRYE( hide_popup() );

            rv = completions[ 0 ];
        }
        else
        {
            KTRYE( show_popup( completions ) );

            rv = longest_common_prefix( completions ); 
        }
    }

    return rv;
}

auto Cli::write( std::string const& out )
    -> void
{
    using emscripten::val;

    KM_RESULT_PROLOG(); 

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

    KM_RESULT_PROLOG(); 

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

    KM_RESULT_PROLOG(); 

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

    KM_RESULT_PROLOG(); 

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
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    auto elem = KTRY( js::fetch_style_member( to_string( canvas->cli_pane() ) ) );

    elem.set( "value", "" );
    elem.set( "backgroundColor", "white" );

    KTRY( hide_popup() );

    rv = outcome::success();

    return rv;
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

    KM_RESULT_PROLOG();

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

    KM_RESULT_PROLOG();

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

    KM_RESULT_PROLOG();

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
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "text", text );

    using emscripten::val;

    auto rv = result::make_result< void >();
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

    rv = outcome::success();

    return rv;
}

auto Cli::show_popup( StringVec const& lines )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const formatted = lines
                         | views::intersperse( std::string{ "<br>" } )
                         | to_vector;

    KTRY( show_popup( formatted
                    | views::join
                    | to< std::string >() ) );

    rv = outcome::success();

    return rv;
}

auto Cli::hide_popup()
    -> Result< void >
{
    using emscripten::val;
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto& km = kmap_inst();
    auto const canvas = KTRY( km.fetch_component< com::Canvas >() );
    auto constexpr code =
R"%%%(
const elem = document.getElementById( '{}' );
elem.hidden = true;
)%%%";

    KTRY( js::eval_void( fmt::format( code, to_string( canvas->completion_overlay() ) ) ) );

    rv = outcome::success();

    return rv;
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
    auto const fn = fmt::format( "cmd_{}", cid );
    
    // TODO: The registration "append_script call" should happen at the callback of writes to body (or leaving body editor).
    //       This function should merely translate the id to a cmd_* function and call it.

    auto append_script = val::global( "append_script" );

    append_script( fn, code );

    fmt::print( "Command {} updated\n", fn );

    return true;
}

auto Cli::notify_success( std::string const& message )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "message", message );

    auto rv = result::make_result< void >();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    KTRY( clear_input() );
    write( fmt::format( "[success] {}", message ) );
    disable_write();
    set_color( Color::green );
    // update_pane();

    rv = outcome::success();

    return rv;
}

auto Cli::notify_failure( std::string const& message )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "message", message );

    auto rv = result::make_result< void >();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    KTRY( clear_input() );
    write( fmt::format( "[failure] {}", message ) );
    disable_write();
    set_color( Color::red );

    rv = outcome::success();

    return rv;
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
            // KTRY( parse_raw( read() ) );
            KTRY( parse_raw( text ) );
            break;
        }
        case 27/*escape*/:
        {
            if( is_ctrl && 67 == key ) // ctrl+c
            {
                KTRY( clear_input() );
                nw->focus();
            }
            break;
        }
        case 67/*c*/:
        {
            if( is_ctrl )
            {
                KTRY( clear_input() );
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
    KM_RESULT_PROLOG();

    KTRYE( parse_raw( input ) );
}

// TODO: Replace update panes with events
auto Cli::update_pane()
    -> void
{
    KM_RESULT_PROLOG();

    auto& km = kmap_inst();
    auto const canvas = KTRYE( km.fetch_component< com::Canvas >() );

    KTRYE( canvas->update_pane( canvas->cli_pane() ) ); // TODO: Find out why focusing requires a repositioning of the CLI element. That is, why calls to this are necessary to maintain dimensions.
}

namespace binding {

using namespace emscripten;

struct Cli
{
    kmap::Kmap& km;

    auto clear_input()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG(); 

        auto const cli = KTRY( km.fetch_component< com::Cli >() );

        return cli->clear_input();
    }
    auto enable_write()
        -> void
    {
        KM_RESULT_PROLOG(); 

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->enable_write();
    }
    auto focus()
        -> void
    {
        KM_RESULT_PROLOG(); 

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->focus();
    }
    auto on_key_down( int const key
                    , bool const is_ctrl
                    , bool const is_shift
                    , std::string const& text )
        -> kmap::Result< void >
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
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        KTRYE( cli->notify_success( message ) );
    }
    auto notify_failure( std::string const& message )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        KTRYE( cli->notify_failure( message ) );
    }
    auto parse_cli( std::string const& input )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->parse_cli( input );
    }
    auto write( std::string const& input )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->write( input );
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
        .function( "clear_input", &kmap::com::binding::Cli::clear_input )
        .function( "enable_write", &kmap::com::binding::Cli::enable_write )
        .function( "focus", &kmap::com::binding::Cli::focus )
        .function( "notify_failure", &kmap::com::binding::Cli::notify_failure )
        .function( "notify_success", &kmap::com::binding::Cli::notify_success )
        .function( "on_key_down", &kmap::com::binding::Cli::on_key_down )
        .function( "parse_cli", &kmap::com::binding::Cli::parse_cli )
        .function( "write", &kmap::com::binding::Cli::write );
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
