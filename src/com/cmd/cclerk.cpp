/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/cmd/cclerk.hpp"

#include "cmd/parser.hpp"
#include "com/cmd/command.hpp"
#include "kmap.hpp"
#include "path/act/fetch_body.hpp"
#include "util/result.hpp"
#include <path/node_view2.hpp>
#include <utility.hpp>
#include <util/clerk/clerk.hpp>

#include <boost/variant/apply_visitor.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>

namespace rvs = ranges::views;

namespace kmap::com {

CommandClerk::CommandClerk( Kmap& km )
    : kmap{ km }
{
}

CommandClerk::~CommandClerk()
{
    if( auto cstorer = kmap.fetch_component< CommandStore >() // TODO: What to do if fails?
      ; cstorer ) 
    {
        auto& cstore = cstorer.value();
        auto const handle_result = []( auto const& res )
        {
            if( !res
             && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
            {
                KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
            }
        };

        for( auto const& e : installed_commands | rvs::values | rvs::reverse ) { handle_result( cstore->uninstall_command( e ) ); }
        for( auto const& e : installed_arguments | rvs::values | rvs::reverse ) { handle_result( cstore->uninstall_argument( e ) ); }
        for( auto const& e : installed_guards | rvs::values | rvs::reverse ) { handle_result( cstore->uninstall_guard( e ) ); }
    }
}

auto CommandClerk::check_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    for( auto const& guard : registered_guards
                           | rvs::values )
    {
        KTRY( check_registered( guard ) );
    }
    for( auto const& arg : registered_arguments
                         | rvs::values )
    {
        KTRY( check_registered( arg ) );
    }
    for( auto const& cmd : registered_commands
                         | rvs::values )
    {
        KTRY( check_registered( cmd ) );
    }
    for( auto const& cmd : registered_command_aliases
                         | rvs::values )
    {
        KTRY( check_registered( cmd ) );
    }

    rv = outcome::success();

    return rv;
}

auto CommandClerk::check_registered( Argument const& arg )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", arg.path );

    auto rv = KMAP_MAKE_RESULT( void );

    if( auto const vnode = view2::cmd::argument_root
                         | view2::direct_desc( arg.path )
      ; vnode | act2::exists( kmap ) )
    {
        auto const matches = [ & ]() -> bool
        {
            return util::match_body_code( kmap, vnode | view2::child( "completion" ), arg.completion )
                && util::match_body_code( kmap, vnode | view2::child( "guard" ), arg.guard )
                && util::match_raw_body( kmap, vnode | view2::child( "description" ), arg.description )
                ;
        }();

        if( !matches )
        {
            auto const reinstall = KTRY( util::confirm_reinstall( "argument", arg.path ) );

            if( reinstall )
            {
                KTRY( view2::cmd::argument_root
                    | view2::direct_desc( arg.path )
                    | act2::erase_node( kmap ) );
                KTRY( install_argument( arg ) ); // Re-install.
            }
        }
    }
    else
    {
        auto const reinstall = KTRY( util::confirm_reinstall( "argument", arg.path ) );

        if( reinstall )
        {
            KTRY( install_argument( arg ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto match_argument( Kmap const& km
                   , Uuid const& argn
                   , Command::Argument const& arg )
    -> bool
{
    auto rv = false;

    if( auto const arg_src = view2::cmd::argument_root
                           | view2::direct_desc( arg.argument_alias )
                           | act2::fetch_node( km )
      ; arg_src )
    {
        rv = view::make( argn )
           | view::child( arg.heading )
           | view::alias( view::Alias::Src{ arg_src.value() } )
           | view::exists( km );
    }

    return rv;
}

auto CommandClerk::check_registered( Command const& cmd )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "cmd_path", cmd.path );

    auto rv = KMAP_MAKE_RESULT( void );

    if( auto const cmdn = view2::cmd::command_root
                        | view2::direct_desc( cmd.path )
                        | act2::fetch_node( kmap )
      ; cmdn )
    {
        auto const vcmd = anchor::node( cmdn.value() );
        auto argn = KTRY( vcmd | view2::child( "argument" ) | act2::fetch_node( kmap ) );
        // TODO: Replace code check with alias check (ensure alias exists)
        auto const matches = [ & ]() -> bool
        {
            auto const guard_src = KTRYB( view2::cmd::guard_root
                                       | view2::direct_desc( cmd.guard )
                                       | act2::fetch_node( kmap ) );
            auto const guard_dst_r = KTRYB( vcmd 
                                          | view2::child( "guard" )
                                          | view2::child 
                                          | view2::resolve 
                                          | act2::fetch_node( kmap ) );
            return ( guard_src == guard_dst_r )
                && ranges::all_of( cmd.arguments, [ & ]( auto const& arg ){ return match_argument( kmap, argn, arg ); } )
                && util::match_body_code( kmap, vcmd | view2::child( "action" ), cmd.action )
                && util::match_raw_body( kmap, vcmd | view2::child( "description" ), cmd.description )
                ;
        }();

        if( !matches )
        {
            auto const reinstall = KTRY( util::confirm_reinstall( "command", cmd.path ) );

            if( reinstall )
            {
                KMAP_LOG_LINE();
                KTRY( anchor::node( cmdn.value() )
                    | act2::erase_node( kmap ) );
                KTRY( install_command( cmd ) ); // Re-install.
            }
        }
    }
    else
    {
        auto const reinstall = KTRY( util::confirm_reinstall( "command", cmd.path ) );

        if( reinstall )
        {
            KMAP_LOG_LINE();
            KTRY( install_command( cmd ) );
        }
    }

    rv = outcome::success();

    return rv;
}

// TODO: Last piece of puzzle before aliasing is enabled.
auto CommandClerk::check_registered( CommandAlias const& ca )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "src_path", ca.src_path );
        KM_RESULT_PUSH_STR( "dst_path", ca.dst_path );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const vdst = view2::cmd::command_root
                    | view2::direct_desc( ca.dst_path );
    auto const vsrc = view2::cmd::command_root
                    | view2::direct_desc( ca.src_path );

    if( vdst | act2::exists( kmap ) )
    {
        if( !( vdst | view2::alias_src( vsrc | view2::cmd::command_children )
                    | act2::exists( kmap ) ) )
        {
            auto const reinstall = KTRY( util::confirm_reinstall( "command_alias", ca.dst_path ) );

            if( reinstall )
            {
                KMAP_LOG_LINE();
                KTRY( vdst | act2::erase_node( kmap ) );
                KTRY( install_command_alias( ca ) ); // Re-install.
            }
        }
    }
    else
    {
        auto const reinstall = KTRY( util::confirm_reinstall( "command_alias", ca.dst_path ) );

        if( reinstall )
        {
            KMAP_LOG_LINE();
            KTRY( install_command_alias( ca ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto CommandClerk::check_registered( Guard const& guard )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", guard.path );

    auto rv = KMAP_MAKE_RESULT( void );

    if( auto const vnode = view2::cmd::guard_root
                         | view2::direct_desc( guard.path )
      ; vnode | act2::exists( kmap ) )
    {
        // KTRY( print_tree( kmap, KTRY( vnode | act2::fetch_node( kmap ) ) ) );
        auto const matches = [ & ]() -> bool
        {
            return util::match_body_code( kmap, vnode | view2::child( "action" ), guard.action )
                && util::match_raw_body( kmap, vnode | view2::child( "description" ), guard.description )
                ;
        }();

        if( !matches )
        {
            auto const reinstall = KTRY( util::confirm_reinstall( "guard", guard.path ) );

            if( reinstall )
            {
                KTRY( view2::cmd::guard_root
                    | view2::direct_desc( guard.path )
                    | act2::erase_node( kmap ) );
                KTRY( install_guard( guard ) ); // Re-install.
            }
        }
    }
    else
    {
        auto const reinstall = KTRY( util::confirm_reinstall( "guard", guard.path ) );

        if( reinstall )
        {
            KTRY( install_guard( guard ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto CommandClerk::install_argument( Argument const& arg )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", arg.path );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto cstore = KTRY( kmap.fetch_component< CommandStore >() );

    auto const argn = KTRY( cstore->install_argument( arg ) );

    installed_arguments.emplace( arg.path, argn );

    rv = argn;

    return rv;
}

auto CommandClerk::install_command( Command const& cmd )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", cmd.path );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto cstore = KTRY( kmap.fetch_component< CommandStore >() );

    auto const cmdn = KTRY( cstore->install_command( cmd ) );

    installed_commands.emplace( cmd.path, cmdn );

    rv = cmdn;

    return rv;
}

auto CommandClerk::install_command_alias( CommandAlias const& ca )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "src_path", ca.src_path );
        KM_RESULT_PUSH_STR( "dst_path", ca.dst_path );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto cstore = KTRY( kmap.fetch_component< CommandStore >() );

    auto const cmdn = KTRY( cstore->install_command_alias( ca ) );

    installed_command_aliases.emplace( ca.dst_path, cmdn );

    rv = cmdn;

    return rv;
}

auto CommandClerk::install_guard( Guard const& guard )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", guard.path );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto cstore = KTRY( kmap.fetch_component< CommandStore >() );

    auto const guardn = KTRY( cstore->install_guard( guard ) );

    installed_guards.emplace( guard.path, guardn );

    rv = guardn;

    return rv;
}

auto CommandClerk::install_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& [ path, arg ] : registered_arguments )
    {
        if( !installed_arguments.contains( path ) )
        {
            KTRY( install_argument( arg ) );
        }
    }
    for( auto const& [ path, cmd ] : registered_guards )
    {
        if( !installed_guards.contains( path ) )
        {
            KTRY( install_guard( cmd ) );
        }
    }
    for( auto const& [ path, cmd ] : registered_commands )
    {
        if( !installed_commands.contains( path ) )
        {
            KTRY( install_command( cmd ) );
        }
    }
    for( auto const& [ path, ca ] : registered_command_aliases )
    {
        if( !installed_command_aliases.contains( path ) )
        {
            KTRY( install_command_alias( ca ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto CommandClerk::register_argument( Argument const& arg ) 
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", arg.path );

    KMAP_ENSURE( !registered_arguments.contains( arg.path ), error_code::common::uncategorized );

    registered_arguments.emplace( arg.path, arg );

    return outcome::success();
}

auto CommandClerk::register_command( Command const& cmd ) 
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", cmd.path );

    KMAP_ENSURE( !registered_commands.contains( cmd.path ), error_code::common::uncategorized );

    registered_commands.emplace( cmd.path, cmd );

    return outcome::success();
}

auto CommandClerk::register_command_alias( CommandAlias const& ca ) 
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "src_path", ca.src_path );
        KM_RESULT_PUSH( "dst_path", ca.dst_path );

    KMAP_ENSURE( !registered_command_aliases.contains( ca.dst_path ), error_code::common::uncategorized );

    registered_command_aliases.emplace( ca.dst_path, ca );

    return outcome::success();
}

auto CommandClerk::register_guard( Guard const& guard ) 
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", guard.path );

    KMAP_ENSURE( !registered_guards.contains( guard.path ), error_code::common::uncategorized );

    registered_guards.emplace( guard.path, guard );

    return outcome::success();
}

} // namespace kmap::com
