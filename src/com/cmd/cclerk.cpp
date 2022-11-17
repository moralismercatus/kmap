/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/cmd/cclerk.hpp"

#include "cmd/parser.hpp"
#include "com/cmd/command.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/fetch_body.hpp"

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
    }
}

auto CommandClerk::check_registered()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

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

    rv = outcome::success();

    return rv;
}

auto match_body_code( Kmap const& km
                    , Uuid const& node
                    , std::string const& content )
    -> bool
{
    auto rv = false;

    if( auto const body = view::make( node )
                        | act::fetch_body( km )
      ; body )
    {
        auto const code = KTRYB( cmd::parser::parse_body_code( body.value() ) );

        boost::apply_visitor( [ & ]( auto const& e )
                            {
                                using T = std::decay_t< decltype( e ) >;

                                if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                                {
                                    // TODO: Beautify kscript? To make comparison consistent regardless of syntax....
                                    //       Or, tokenize kscript and compare. May be better.
                                    rv = ( content == e.code );
                                }
                                else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                                {
                                    rv = ( js::beautify( content ) == js::beautify( e.code ) ); 
                                }
                                else
                                {
                                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                }
                            }
                            , code );
    }

    return rv;
}

auto match_raw_body( Kmap const& km
                   , Uuid const& node
                   , std::string const& content )
    -> bool
{
    auto rv = false;

    if( auto const body = view::make( node )
                        | act::fetch_body( km )
      ; body )
    {
        rv = ( body.value() == content );
    }

    return rv;
}

auto CommandClerk::check_registered( Argument const& arg )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    auto const cstore = KTRY( kmap.fetch_component< com::CommandStore >() );
    auto const aroot = KTRY( cstore->argument_root() );
    if( auto const anode = view::make( aroot )
                         | view::direct_desc( arg.path )
                         | view::fetch_node( kmap )
      ; anode )
    {
        auto const vnode = view::make( anode.value() );
        auto const matches = match_body_code( kmap, KTRY( vnode | view::child( "completion" ) | view::fetch_node( kmap ) ), arg.completion )
                          && match_body_code( kmap, KTRY( vnode | view::child( "guard" ) | view::fetch_node( kmap ) ), arg.guard )
                          && match_raw_body( kmap, KTRY( vnode | view::child( "description" ) | view::fetch_node( kmap ) ), arg.description )
                           ;

        if( !matches )
        {
            auto const reinstall = KTRY( js::eval< bool >( fmt::format( "return confirm( \"Argument '{}' out of date.\\nRe-install argument?\" );", arg.path ) ) );

            if( reinstall )
            {
                KTRY( view::make( aroot )
                    | view::direct_desc( arg.path )
                    | view::erase_node( kmap ) );
                KTRY( install_argument( arg ) ); // Re-install.
            }
        }
    }
    else
    {
        auto const reinstall = KTRY( js::eval< bool >( fmt::format( "return confirm( \"Argument '{}' out of date.\\nRe-install argument?\" );", arg.path ) ) );

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

    if( auto const arg_src = view::abs_root
                           | view::direct_desc( "meta.setting.argument" )
                           | view::direct_desc( arg.argument_alias )
                           | view::fetch_node( km )
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
    auto rv = KMAP_MAKE_RESULT( void );

    auto const cstore = KTRY( kmap.fetch_component< com::CommandStore >() );
    auto const croot = KTRY( cstore->command_root() );
    if( auto const cnode = view::make( croot )
                         | view::direct_desc( cmd.path )
                         | view::fetch_node( kmap )
      ; cnode )
    {
        auto vguardn = view::make( cnode.value() ) | view::child( cmd.guard.heading );
        auto argn = KTRY( vguardn | view::child( "argument" ) | view::fetch_node( kmap ) );
        auto const matches = match_body_code( kmap, KTRY( vguardn | view::fetch_node( kmap ) ), cmd.guard.code )
                          && match_body_code( kmap, KTRY( vguardn | view::child( "action" ) | view::fetch_node( kmap ) ), cmd.action )
                          && ranges::all_of( cmd.arguments, [ & ]( auto const& arg ){ return match_argument( kmap, argn, arg ); } )
                          && match_raw_body( kmap, KTRY( vguardn | view::child( "description" ) | view::fetch_node( kmap ) ), cmd.description )
                           ;

        if( !matches )
        {
            auto const reinstall = KTRY( js::eval< bool >( fmt::format( "return confirm( \"Command '{}' out of date.\\nRe-install command?\" );", cmd.path ) ) );

            if( reinstall )
            {
                KTRY( view::make( cnode.value() )
                    | view::erase_node( kmap ) );
                KTRY( install_command( cmd ) ); // Re-install.
            }
        }
    }
    else
    {
        auto const reinstall = KTRY( js::eval< bool >( fmt::format( "return confirm( \"Command '{}' out of date.\\nRe-install command?\" );", cmd.path ) ) );

        if( reinstall )
        {
            KTRY( install_command( cmd ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto CommandClerk::install_argument( Argument const& arg )
    -> Result< Uuid >
{
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
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto cstore = KTRY( kmap.fetch_component< CommandStore >() );

    auto const cmdn = KTRY( cstore->install_command( cmd ) );

    installed_commands.emplace( cmd.path, cmdn );

    rv = cmdn;

    return rv;
}

auto CommandClerk::install_registered()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& [ path, arg ] : registered_arguments )
    {
        if( !installed_arguments.contains( path ) )
        {
            KTRY( install_argument( arg ) );
        }
    }
    for( auto const& [ path, cmd ] : registered_commands )
    {
        if( !installed_commands.contains( path ) )
        {
            KTRY( install_command( cmd ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto CommandClerk::register_argument( Argument const& arg ) 
    -> void
{
    registered_arguments.emplace( arg.path, arg );
}

auto CommandClerk::register_command( Command const& cmd ) 
    -> void
{
    registered_commands.emplace( cmd.path, cmd );
}

} // namespace kmap::com
