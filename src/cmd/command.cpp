/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "command.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../emcc_bindings.hpp"
#include "../error/network.hpp"
#include "../error/master.hpp"
#include "../io.hpp"
#include "../io.hpp"
#include "../js_iface.hpp"
#include "../kmap.hpp"
#include "../path.hpp"
#include "parser.hpp"
#include "script.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/split.hpp>

#include <sstream>

using namespace ranges;

namespace kmap::cmd {

auto is_general_command( Kmap const& kmap
                       , Uuid const& id )
    -> bool
{
    auto rv = bool{};

    if( auto const cmd_root = kmap.fetch_descendant( "/meta.settings.commands" )
      ; cmd_root )
    {
        rv = kmap.has_descendant( id
                                , [ &kmap, &id ]( auto const& node )
        {
            return kmap.distance( id, node ) == 1
                && is_particular_command( kmap, node );
        } );
    }

    return rv;
}

auto is_particular_command( Kmap const& kmap
                          , Uuid const& id )
    -> bool
{
    auto rv = bool{};

    if( auto const cmd_root = kmap.fetch_descendant( "/meta.settings.commands" )
      ; cmd_root )
    {
        rv = kmap.is_lineal( cmd_root.value()
                           , id )
          && kmap.is_child( id
                          , "description"
                          , "arguments"
                          , "action" );
    }

    return rv;
}

auto is_argument( Kmap const& kmap
                , Uuid const& id )
    -> bool
{
    auto rv = bool{};

    if( auto const cmd_root = kmap.fetch_leaf( "/meta.settings.arguments" ) )
    {
        rv = kmap.is_lineal( *cmd_root
                           , id )
          && kmap.is_child( id
                          , "description"
                          , "guard"
                          , "completion" );
    }

    return rv;
}

auto to_command_name( Uuid const& id )
    -> std::string
{
    auto const sid = to_string( id );
    auto const cid = sid
                   | views::remove( '-' )
                   | to< std::string >();
    auto const fn = fmt::format( "cmd_{}"
                               , cid );

    return fn;
}

auto execute_kscript( Kmap& kmap 
                    , std::string const& code )
    -> CliCommandResult
{
    auto rv = CliCommandResult{ CliResultCode::failure
                              , "unknown failure" };
    auto ss = std::stringstream{ code };

    rv = load_script( kmap, ss );

    return rv;
}

auto execute_javascript( Uuid const& node
                       , std::string_view const fn_body
                       , StringVec const& args )
    -> CliCommandResult
{
    using emscripten::val;

    auto rv = CliCommandResult{ CliResultCode::failure
                              , "unknown failure" };
    auto const fn_name = fmt::format( "fn_{}"
                                    , format_heading( to_string( node ) ) );


    if( auto const fn_created = js::publish_function( fn_name, { "args" }, fn_body )
      ; fn_created )
    {
        if( auto crv = js::call< binding::Result< std::string > >( fn_name, args )
          ; crv )
        {
            if( crv->has_value() )
            {
                rv.result = CliResultCode::success;
                rv.message = crv->value();
            }
            else
            {
                rv.result = CliResultCode::failure;
                rv.message = crv->failure_message.value_or( crv->error_message() );
            }
        }
        else
        {
            EM_ASM
            (
                throw "Unknown type returned from guard function; expected kmap.Result<std::string>";
            );
        }
    }
    else
    {
        rv.result = CliResultCode::failure;
        rv.message = "function publication failed";
    }

    return rv;
}

auto execute_javascript( Uuid const& node
                       , std::string_view const fn_body
                       , std::string const& arg )
    -> CliCommandResult
{
    using emscripten::val;

    auto rv = CliCommandResult{ CliResultCode::failure
                              , "unknown failure" };
    auto const fn_name = fmt::format( "fn_{}"
                                    , format_heading( to_string( node ) ) );


    if( auto const fn_created = js::publish_function( fn_name, { "arg" }, fn_body )
      ; fn_created )
    {
        if( auto crv = js::call< binding::Result< std::string > >( fn_name, arg )
          ; crv )
        {
            if( crv->has_value() )
            {
                rv.result = CliResultCode::success;
                rv.message = crv->value();
            }
            else
            {
                rv.result = CliResultCode::failure;
                rv.message = crv->failure_message.value_or( crv->error_message() );
            }
        }
        else
        {
            EM_ASM
            (
                throw "Unknown type returned from guard function; expected kmap.Result<std::string>";
            );
        }
    }
    else
    {
        rv.result = CliResultCode::failure;
        rv.message = "function publication failed";
    }

    return rv;
}

auto execute_body( Kmap& kmap
                 , Uuid const& node
                 , StringVec const& args )
    -> CliCommandResult
{
    auto rv = CliCommandResult{ CliResultCode::failure
                              , "failed to execute body" };

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( node ) );
        })
    ;

    if( auto const body = kmap.fetch_body( node )
      ; body )
    {
        if( auto const code = parser::parse_body_code( body.value() )
          ; code )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                {
                    rv = execute_kscript( kmap
                                        , e.code );
                }
                else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                {
                    rv = execute_javascript( node
                                           , e.code
                                           , args );
                }
                else
                {
                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                }
            }
            , *code );
        }
        else
        {
            // TODO: set error_code appropriately.
            io::print( stderr, "parse_body_code failed\n" );
        }
    }
    
    return rv;
}

auto evaluate_guard( Kmap& kmap
                   , Uuid const& guard_node
                   , std::string const& arg )
    -> CliCommandResult
{
    auto rv = CliCommandResult{ CliResultCode::failure
                              , "unknown failuare" };

    if( auto const body = kmap.fetch_body( guard_node )
      ; body )
    {
        if( auto const code = cmd::parser::parse_body_code( body.value() )
          ; code )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                {
                    rv.message = "kscript is not supported in a guard node";
                }
                else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                {
                    rv = execute_javascript( guard_node
                                           , e.code
                                           , arg );
                }
                else
                {
                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                }
            }
            , *code );
        }
    }

    return rv;
}

auto evaluate_completer( Kmap& kmap
                       , Uuid const& completer_node
                       , std::string const& arg )
    -> Result< StringVec >
{
    auto rv = KMAP_MAKE_RESULT( StringVec );

    if( auto const body = kmap.fetch_body( completer_node )
      ; body )
    {
        if( auto const code = cmd::parser::parse_body_code( body.value() )
          ; code )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                {
                    rv = KMAP_MAKE_ERROR( error_code::command::kscript_unsupported );
                }
                else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                {
                    auto const fn_name = fmt::format( "fn_{}", format_heading( to_string( completer_node ) ) );

                    if( auto const fn_created = js::publish_function( fn_name, { "arg" }, e.code )
                      ; fn_created )
                    {
                        if( auto const crv = js::call< StringVec >( fn_name, arg )
                          ; crv )
                        {
                            rv = crv.value();
                        }
                        else
                        {
                            EM_ASM
                            (
                                throw "Unknown type returned from guard function; expected kmap.VectorString";
                            );
                        }
                    }
                    else
                    {
                        rv = KMAP_MAKE_ERROR( error_code::command::fn_publication_failed );
                    }
                }
                else
                {
                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                }
            }
            , *code );
        }
    }

    return rv;
}

auto has_unconditional_arg( Kmap const& kmap
                          , Uuid const& cmd
                          , Uuid const& args_root )
    -> bool
{
    auto rv = false;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( is_particular_command( kmap, cmd ) );
        })
    ;

    auto const unconditional = kmap.fetch_descendant( args_root, "/unconditional" ).value(); // TODO: This is dangerous. User could delete "unconditional".
    auto const arg_node = kmap.fetch_descendant( cmd, "/arguments" ).value();
    auto const children = kmap.fetch_children( arg_node );

    for( auto const& arg_descr : children )
    {
        if( auto const descr_children = kmap.fetch_children( arg_descr )
          ; descr_children.size() == 1 )
        {
            auto const arg = *descr_children.begin();

            rv = kmap.resolve( arg ) == unconditional;

            if( rv )
            {
                break;
            }
        }
    }

    return rv;
}

auto fetch_args( Kmap& kmap
               , Uuid const& cmd_id
               , std::string const& arg )
    -> Result< StringVec >
{
    auto rv = KMAP_MAKE_RESULT( StringVec );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( is_particular_command( kmap, cmd_id ) );
            // TODO: once node deletion/movement is restricted for arguments, safe to assert their root's existence and depend on it existing.
        })
    ;

    auto const arg_nodes = kmap.fetch_children_ordered( kmap.node_view( cmd_id )[ "/arguments" ] );

    if( auto const args_root = kmap.fetch_descendant( "/meta.settings.arguments" ) // TODO: Eventually, this check should be superfluous when this node is made immutable.
      ; args_root )
    {
        auto const split_args = arg
                              | views::split( ' ' ); // Note the lazy split. Important for large strings that are actually unconditional, for performance.

        if( has_unconditional_arg( kmap
                                 , cmd_id
                                 , args_root.value() ) 
         || distance( split_args ) == arg_nodes.size() )
        {
            if( arg_nodes.size() == 0 )
            {
                rv = StringVec{};
            }
            else
            {
                auto const unconditional = kmap.node_view( args_root.value() )[ "/unconditional" ]; // TODO: Can't assume user didn't delete this node, at present time.
                auto validated_args = StringVec{}; // TODO: Is this even of use?

                for( auto const [ index, sarg ] : split_args | views::enumerate )
                {
                    BC_ASSERT( index < arg_nodes.size() );

                    auto const arg_descr = arg_nodes[ index ]; // A descriptor node preceds the aliased argument.
                    auto const arg_descr_children = kmap.fetch_children( arg_descr );

                    if( arg_descr_children.size() == 1 )
                    {
                        if( auto const arg_node = *arg_descr_children.begin()
                          ; is_argument( kmap, kmap.resolve( arg_node ) ) )
                        {
                            if( kmap.resolve( arg_node ) == unconditional )
                            {
                                io::print( "arg is unconditinoal\n" );
                                validated_args.emplace_back( split_args
                                                           | views::drop( index )
                                                           | views::join( ' ' )
                                                           | to< std::string >() );

                                rv = validated_args;

                                break;
                            }
                            else
                            {
                                auto const arg_view = kmap.node_view( arg_node );
                                auto const validity_check = arg_view[ "/guard" ];

                                if( auto const arg_res = evaluate_guard( kmap, validity_check, sarg | to< std::string >() )
                                  ; !arg_res )
                                {
                                    rv = KMAP_MAKE_ERROR( error_code::command::invalid_arg );

                                    // TODO: need to provide error info in payload of why guard failed.
                                    io::print( "argument guard failed: {}\n"
                                             , index );

                                    break;
                                }
                                else
                                {
                                    // TODO: push arg into rv vector.
                                    validated_args.emplace_back( sarg | to< std::string >() );

                                    rv = validated_args;
                                }
                            }
                        }
                        else
                        {
                            rv = KMAP_MAKE_ERROR( error_code::command::nonarg_node_found );
                        }
                    }
                    else
                    {
                        rv = KMAP_MAKE_ERROR( error_code::command::invalid_arg ); // TODO: this should be more descriptive.

                        break;
                    }

                }
            }
        }
        else
        {
            rv = KMAP_MAKE_ERROR( error_code::command::incorrect_arg_number );
            // TODO: Give exact expected numbers:
            // rv.errer().payload = fmt::format( "expected between [{},{}] arguments, got {}"
            //                         , high
            //                         , low
            //                         , distance( split_args ) );
        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::node::not_found );
        // TODO: rv.error().payload ="no settings.argument nodes found";
    }

    return rv;
}

auto fetch_params_ordered( Kmap& kmap
                         , Uuid const& cmd_id )
    -> Result< UuidVec >
{
    auto rv = KMAP_MAKE_RESULT( UuidVec );

    KMAP_ENSURE( rv, kmap.exists( cmd_id ), error_code::network::invalid_node );

    auto params = UuidVec{};
    auto const children = kmap.fetch_children_ordered( cmd_id, "arguments" );

    for( auto const& child : children )
    {
        auto const param = kmap.fetch_children( child );

        KMAP_ENSURE( rv, param.size() == 1, error_code::command::invalid_arg );

        params.emplace_back( *param.begin() );
    }

    rv = params;

    return rv;
}

auto execute_command( Kmap& kmap
                    , Uuid const& cmd_id
                    , std::string const& arg )
    -> CliCommandResult 
{
    auto rv = CliCommandResult{ CliResultCode::failure
                              , "failed to execute command" };

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( !val::global( to_command_name( cmd_id ).c_str() ).isUndefined() );
        })
    ;

    if( is_particular_command( kmap, cmd_id ) ) 
    {
        auto const args = fetch_args( kmap, cmd_id, arg );

        if( args )
        {
            auto const action = kmap.node_view( cmd_id )[ "/action" ];

            rv = execute_body( kmap, action, args.value() );
        }
        else
        {
            io::print( stderr, "{}\n", to_string( args.error() ) );

            rv.message = args.error().ec.message();
        }
    }
    else
    {
        rv = CliCommandResult{ CliResultCode::failure
                             , "not a command node" };
    }

    return rv;
}

auto execute_command( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 || args.size() == 1 );
            })
        ;

        auto const target = [ & ]
        {
            if( args.empty() )
            {
                return Optional< Uuid >{ kmap.selected_node() };
            }
            else
            {
                auto const heading = args[ 0 ];

                return kmap.fetch_leaf( heading );
            }
        }();

        if( target )
        {
            return execute_command( kmap
                                  , *target
                                  , "" );
        }
        else
        {
            return { CliResultCode::failure
                   , fmt::format( "target not found" ) };
        }
    };
}

namespace create_command_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( "success" );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const path = args.get( 0 );
const cmd = kmap.create_command( path );

if( cmd.has_value() )
{
    kmap.select_node( cmd.value() );

    rv = kmap.success( "command created" );
}
else
{
    rv = kmap.failure( cmd.error_mesage() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates command template at selected categatory, or root category";
auto const arguments = std::vector< Argument >{ Argument{ "command_path"
                                                        , "path to command"
                                                        , "command_heading_path" } };
auto const guard = PreregisteredCommand::Guard{ "unconditional"
                                              , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.command
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace create_command_def

namespace command_heading_path_def {
namespace {

auto const guard_code =
R"%%%(```javascript
return kmap.is_valid_heading_path( arg );
```)%%%";
auto const completion_code =
R"%%%(```javascript
let rv = new kmap.VectorString();
const root = kmap.fetch_node( '.root.meta.settings.commands' );

if( root.has_value() )
{
    rv = kmap.complete_heading_path_from( root.value(), arg );
}

return rv;
```)%%%";

auto const description = "bookmark heading";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    command_heading_path
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace command_heading_path_def

namespace command::binding {

using namespace emscripten;

auto create_command( std::string const& path )
    -> kmap::binding::Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& kmap = Singleton::instance();
    auto const prereg = PreregisteredCommand{ path
                                            , "Undescribed"
                                            , {}
                                            , { "unconditional", "```javascript\nreturn kmap.success( 'unconditional' );\n```" }
                                            , "```kscript\n:echo Command Unimplemented\n```" };
    auto const cmd = kmap.cli().create_command( prereg );

    rv = cmd;

    return rv;
}

auto fetch_nearest_command( Uuid const& node )
    -> kmap::binding::Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const& kmap = Singleton::instance();
    auto const cmd_root = KMAP_TRY( kmap.fetch_descendant( "/meta.settings.commands" ) );

    auto parent = Optional< Uuid >( node );

    while( parent
        && ( parent.value() != cmd_root ) )
    {
        auto const p = parent.value();

        if( is_general_command( kmap, p ) )
        {
            rv = p;
        }
        else
        {
            parent = to_optional( kmap.fetch_parent( p ) );
        }
    }

    return rv;
}

EMSCRIPTEN_BINDINGS( kmap_module )
{
    function( "create_command", &kmap::cmd::command::binding::create_command );
    function( "fetch_nearest_command", &kmap::cmd::command::binding::fetch_nearest_command );
}

} // namespace command::binding

} // namespace kmap::cmd