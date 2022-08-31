/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "command.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../emcc_bindings.hpp"
#include "../error/master.hpp"
#include "../error/network.hpp"
#include "../io.hpp"
#include "../io.hpp"
#include "../js_iface.hpp"
#include "../kmap.hpp"
#include "../path.hpp"
#include "com/cmd/command.hpp"
#include "com/network/network.hpp"
#include "parser.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"
#include "script.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <range/v3/action/join.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>

#include <sstream>

using namespace ranges;

namespace kmap::cmd {

auto is_general_command( Kmap const& kmap
                       , Uuid const& id )
    -> bool
{
    auto rv = bool{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( auto const cmd_root = view::make( kmap.root_node_id() )
                            | view::direct_desc( "meta.setting.command" )
                            | view::fetch_node( kmap )
      ; cmd_root )
    {
        rv = view::make( id )
           | view::desc( view::PredFn{ [ &kmap, &nw, &id ]( auto const& node )
                                       {
                                           return nw->distance( id, node ) == 1
                                               && is_particular_command( kmap, node );
                                       } } )
           | view::exists( kmap );
    }

    return rv;
}

auto is_particular_command( Kmap const& kmap
                          , Uuid const& id )
    -> bool
{
    auto rv = bool{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( auto const cmd_root = view::make( kmap.root_node_id() )
                            | view::direct_desc( "meta.setting.command" )
                            | view::fetch_node( kmap )
      ; cmd_root )
    {
        rv = nw->is_lineal( cmd_root.value(), id )
          && nw->is_child( id
                         , "description"
                         , "argument"
                         , "action" );
    }

    return rv;
}

auto is_argument( Kmap const& km
                , Uuid const& id )
    -> bool
{
    auto rv = bool{};
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    if( auto const cmd_root = view::abs_root
                            | view::direct_desc( "meta.setting.argument" )
                            | view::fetch_node( km )
      ; cmd_root )
    {
        rv = nw->is_lineal( cmd_root.value(), id )
          && nw->is_child( id
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
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto ss = std::stringstream{ code };

    rv = load_script( kmap, ss );

    return rv;
}

auto execute_javascript( Uuid const& node
                       , std::string_view const fn_body
                       , StringVec const& args )
    -> Result< std::string >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const fn_name = fmt::format( "fn_{}"
                                    , format_heading( to_string( node ) ) );
    auto const stringed = args
                        | views::transform( []( auto const& arg ){ return io::format( "'{}'", boost::replace_all_copy( arg, "'", "\\'" ) ); } )
                        | to< StringVec >();
    auto const csep = stringed
                    | views::join( ',' )
                    | to< std::string >();

    io::print( "execute_javascript.args: {}\n", csep );

    KMAP_TRY( js::publish_function( fn_name, { "args" }, fn_body ) );

    rv = KMAP_TRY( js::eval< binding::Result< std::string > >( io::format( "return {}( to_VectorString( [{}] ) );"
                                                                         , fn_name
                                                                         , csep ) ) );

    return rv;
}

auto execute_javascript( Uuid const& node
                       , std::string_view const fn_body
                       , std::string const& arg )
    -> Result< std::string >
{
    using emscripten::val;

    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const fn_name = fmt::format( "fn_{}"
                                    , format_heading( to_string( node ) ) );
    KMAP_TRY( js::publish_function( fn_name, { "arg" }, fn_body ) );
    io::print( "executing: {}\n", io::format( "{}( '{}' );", fn_name, arg )  );
    rv = KMAP_TRY( js::eval< binding::Result< std::string > >( io::format( "return {}( '{}' );", fn_name, arg ) ) );

    return rv;
}

auto execute_body( Kmap& kmap
                 , Uuid const& node
                 , StringVec const& args )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( view::make( node ) | view::exists( kmap ) );
        })
    ;

    auto const body = KMAP_TRY( nw->fetch_body( node ) );
    auto const code = KMAP_TRY( parser::parse_body_code( body ) );

    boost::apply_visitor( [ & ]( auto const& e )
                        {
                            using T = std::decay_t< decltype( e ) >;

                            if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                            {
                                rv = execute_kscript( kmap, e.code );
                            }
                            else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                            {
                                rv = execute_javascript( node, e.code, args );
                            }
                            else
                            {
                                static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                            }
                        }
                        , code );
    
    return rv;
}

// TODO: The fact that kmap is const, but executing the script could result in non-const actions is concerning to me.
auto evaluate_guard( Kmap const& kmap
                   , Uuid const& guard_node
                   , std::string const& arg )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    auto const body = KMAP_TRY( nw->fetch_body( guard_node ) );
    auto const code = KMAP_TRY( cmd::parser::parse_body_code( body ) );

    boost::apply_visitor( [ & ]( auto const& e )
                        {
                            using T = std::decay_t< decltype( e ) >;

                            if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                            {
                                rv = "kscript is not supported in a guard node";
                            }
                            else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                            {
                                auto const escaped_arg = boost::replace_all_copy( arg, "'", "\\'" );

                                rv = execute_javascript( guard_node
                                                       , e.code
                                                       , escaped_arg );
                            }
                            else
                            {
                                static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                            }
                        }
                        , code );

    return rv;
}

auto evaluate_completer( Kmap& kmap
                       , Uuid const& completer_node
                       , std::string const& arg )
    -> Result< StringVec >
{
    auto rv = KMAP_MAKE_RESULT( StringVec );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const body = KMAP_TRY( nw->fetch_body( completer_node ) );
    auto const code = KMAP_TRY( cmd::parser::parse_body_code( body ) );

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
    , code );

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

    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const unconditional = KTRYE( view::make( args_root )
                                    | view::child( "unconditional" )
                                    | view::fetch_node( kmap ) );
    auto const arg_node = KTRYE( view::make( cmd )
                               | view::child( "argument" )
                               | view::fetch_node( kmap ) );
    auto const children = nw->fetch_children( arg_node );

    for( auto const& arg_descr : children )
    {
        if( auto const descr_children = nw->fetch_children( arg_descr )
          ; descr_children.size() == 1 )
        {
            auto const arg = *descr_children.begin();

            rv = nw->resolve( arg ) == unconditional;

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

    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const arg_node = KTRY( view::make( cmd_id ) | view::child( "argument" ) | view::fetch_node( kmap ) );
    auto const arg_nodes = view::make( arg_node )
                         | view::child
                         | view::to_node_set( kmap )
                         | act::order( kmap );

    if( auto const args_root = view::make( kmap.root_node_id() )
                             | view::direct_desc( "meta.setting.argument" )
                             | view::fetch_node( kmap )
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
                auto const unconditional = KTRY( view::make( args_root.value() ) | view::child( "unconditional" ) | view::fetch_node( kmap ) );
                auto validated_args = StringVec{}; // TODO: Is this even of use?

                for( auto const [ index, sarg ] : split_args | views::enumerate )
                {
                    BC_ASSERT( index < arg_nodes.size() );

                    auto const arg_descr = arg_nodes[ index ]; // A descriptor node preceds the aliased argument.
                    auto const arg_descr_children = nw->fetch_children( arg_descr );

                    if( arg_descr_children.size() == 1 )
                    {
                        if( auto const arg_node = *arg_descr_children.begin()
                          ; is_argument( kmap, nw->resolve( arg_node ) ) )
                        {
                            if( nw->resolve( arg_node ) == unconditional )
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
                                auto const validity_check = KTRY( view::make( arg_node )
                                                                | view::child( "guard" )
                                                                | view::fetch_node( kmap ) );

                                if( auto const arg_res = evaluate_guard( kmap, validity_check, sarg | to< std::string >() )
                                  ; !arg_res )
                                {
                                    rv = KMAP_MAKE_ERROR_MSG( error_code::command::invalid_arg,  to_string( arg_res.error() ) );

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
            rv = KMAP_MAKE_ERROR_MSG( error_code::command::incorrect_arg_number
                                    , fmt::format( "expected between [{},{}] arguments, got {}"
                                                 , arg_nodes.size() // TODO: Need to determine optional args.
                                                 , arg_nodes.size()
                                                 , distance( split_args ) ) );
        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::node::not_found );
        // TODO: rv.error().payload ="no setting.argument nodes found";
    }

    return rv;
}

auto fetch_params_ordered( Kmap& kmap
                         , Uuid const& cmd_id )
    -> Result< UuidVec >
{
    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    KMAP_ENSURE( nw->exists( cmd_id ), error_code::network::invalid_node );

    auto params = UuidVec{};
    auto const children = view::make( cmd_id )
                        | view::child( "argument" )
                        | view::child
                        | view::to_node_set( kmap )
                        | act::order( kmap );

    for( auto const& child : children )
    {
        auto const param = nw->fetch_children( child );

        KMAP_ENSURE( param.size() == 1, error_code::command::invalid_arg );

        params.emplace_back( *param.begin() );
    }

    rv = params;

    return rv;
}

auto execute_command( Kmap& kmap
                    , Uuid const& cmd_id
                    , std::string const& arg )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( !val::global( to_command_name( cmd_id ).c_str() ).isUndefined() );
        })
    ;

    if( is_particular_command( kmap, cmd_id ) ) 
    {
        auto const args = KMAP_TRY( fetch_args( kmap, cmd_id, arg ) );
        auto const action = KTRY( view::make( cmd_id )
                                | view::child( "action" )
                                | view::fetch_node( kmap ) );

         rv = KMAP_TRY( execute_body( kmap, action, args ) );
    }
    else
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "not a command node" );
    }

    return rv;
}

auto execute_command( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 || args.size() == 1 );
            })
        ;

        auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
        auto const target = [ & ]
        {
            if( args.empty() )
            {
                return Result< Uuid >{ nw->selected_node() };
            }
            else
            {
                auto const heading = args[ 0 ];

                return view::make( kmap.root_node_id() )
                     | view::desc( heading )
                     | view::fetch_node( kmap );
            }
        }();

        if( target )
        {
            return execute_command( kmap
                                  , target.value()
                                  , "" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "target not found" ) );
        }
    };
}

#if 0
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

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "creates command template at selected categatory, or root category";
auto const arguments = std::vector< Argument >{ Argument{ "command_path"
                                                        , "path to command"
                                                        , "command_heading_path" } };
auto const guard = com::Command::Guard{ "unconditional", guard_code };
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
#endif // 0

#if 0
namespace command_heading_path_def {
namespace {

auto const guard_code =
R"%%%(```javascript
return kmap.is_valid_heading_path( arg );
```)%%%";
auto const completion_code =
R"%%%(```javascript
let rv = new kmap.VectorString();
const root = kmap.fetch_node( '.root.meta.setting.command' );

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
#endif // 0

namespace command::binding {

using namespace emscripten;

auto create_command( std::string const& path )
    -> kmap::binding::Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    // auto& kmap = Singleton::instance();
    auto const prereg = com::Command{ path
                                    , "Undescribed"
                                    , {}
                                    , { "unconditional", "```javascript\nreturn kmap.success( 'unconditional' );\n```" }
                                    , "```kscript\n:echo Command Unimplemented\n```" };
    // auto const cmd = kmap.cli().create_command( prereg );

    // rv = cmd;

    return rv;
}

auto fetch_nearest_command( Uuid const& node )
    -> kmap::binding::Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const& kmap = Singleton::instance();
    auto const cmd_root = KTRY( view::make( kmap.root_node_id() )
                              | view::direct_desc( "meta.setting.command" )
                              | view::fetch_node( kmap ) );

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
            auto const nw = KTRY( kmap.fetch_component< com::Network >() );

            parent = to_optional( nw->fetch_parent( p ) );
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