/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/cmd/command.hpp"

#include "com/network/network.hpp"
#include "error/js_iface.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/abs_path.hpp"
#include "path/act/back.hpp"
#include "path/act/erase.hpp"
#include "path/act/update_body.hpp"
#include "path/node_view.hpp"

namespace kmap::com {

auto CommandStore::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( install_standard_arguments() );

    rv = outcome::success();

    return rv;
}

auto CommandStore::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto CommandStore::argument_root()
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    rv = KTRY( view::make( km.root_node_id() )
             | view::direct_desc( "meta.setting.argument" )
             | view::fetch_or_create_node( km ) );

    return rv;
}

auto CommandStore::command_root()
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    rv = KTRY( view::make( km.root_node_id() )
             | view::direct_desc( "meta.setting.command" )
             | view::fetch_or_create_node( km ) );

    return rv;
}

auto CommandStore::install_argument( Argument const& arg )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    
    auto const arg_root = KTRY( view::make( km.root_node_id() )
                              | view::direct_desc( "meta.setting.argument" )
                              | view::fetch_or_create_node( km ) );
    auto const vargn = view::make( arg_root ) 
                     | view::direct_desc( arg.path );
    
    if( vargn | view::exists( km ) )
    {
        KTRY( vargn | act::erase( km ) );
    } 

    {
        // auto const argn = KTRY( vargn | view::fetch_or_create_node( km ) );
        KTRY( vargn
            | view::child( view::all_of( "description", "guard", "completion" ) )
            | view::create_node( km ) );
        
        KTRY( vargn | view::child( "description" ) | act::update_body( km, arg.description ) );
        KTRY( vargn | view::child( "guard" ) | act::update_body( km, arg.guard ) );
        KTRY( vargn | view::child( "completion" ) | act::update_body( km, arg.completion ) );

        rv = KTRY( vargn | view::fetch_node( km ) );
    }

    return rv;
}


auto CommandStore::install_command( Command const& cmd )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    // KMAP_ENSURE( js::lint( cmd.guard.code ), error_code::js::lint_failed ); // Err... need to parse code from ```javascript ... ``` style text wrapper.
    // KMAP_ENSURE( js::lint( cmd.action ), error_code::js::lint_failed );

    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const cmd_root = KTRY( command_root() );
    auto const vguard = view::make( cmd_root )
                      | view::direct_desc( cmd.path )
                      | view::child( cmd.guard.heading );

    KTRY( vguard | view::create_node( km ) );

    auto const guardn = KTRY( vguard | view::fetch_node( km ) );

    KTRY( view::make( guardn )
        | view::child( view::all_of( "description"
                                   , "argument"
                                   , "action" ) )
        | view::create_node( km ) );

    auto const descn = KTRY( vguard | view::child( "description" ) | view::fetch_node( km ) );
    
    KTRY( nw->update_body( guardn, cmd.guard.code ) );
    KTRY( nw->update_body( descn, cmd.description ) );

    for( auto const& arg : cmd.arguments )
    {
        auto const argn = KTRY( vguard | view::child( "argument" ) | view::fetch_node( km ) );
        auto const arg_src = KTRY( km.root_view()
                                 | view::direct_desc( "meta.setting.argument" )
                                 | view::direct_desc( arg.argument_alias)
                                 | view::fetch_node( km ) );
        auto const arg_dst = KTRY( nw->create_child( argn, arg.heading ) );
        auto const nw = KTRY( fetch_component< com::Network >() );

        KTRY( nw->update_body( arg_dst, arg.description ) );
        KTRY( nw->alias_store().create_alias( arg_src, arg_dst ) );
    }

    auto const actn = KTRY( vguard
                          | view::child( "action" )
                          | view::fetch_node( km ) );

    KTRY( nw->update_body( actn, cmd.action ) );

    rv = guardn;

    return rv;
}

// TODO: Should this be a separate component? E.g., "cmd.standard_arguments"?
auto CommandStore::install_standard_arguments()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // arg.unconditional
    {
        auto const guard_code =
R"%%%(```javascript
return true;
```)%%%";
        auto const completion_code =
R"%%%(```javascript
let rv = new kmap.VectorString();

rv.push_back( arg )

return rv;
```)%%%";

        auto const description = "any valid text";

        KTRY( install_argument( Argument{ .path = "unconditional"
                                        , .description = description
                                        , .guard = guard_code
                                        , .completion = completion_code } ) );
    }
    // arg.filesystem_path // TODO: I suspect that this belongs in a "filesystem" component.
    {
        auto const guard_code =
R"%%%(```javascript
return kmap.success( "success" );
```)%%%";
        auto const completion_code =
R"%%%(```javascript
return kmap.complete_filesystem_path( arg );
```)%%%";

        auto const description = "filesystem path";

        KTRY( install_argument( Argument{ .path = "filesystem_path"
                                        , .description = description
                                        , .guard = guard_code
                                        , .completion = completion_code } ) );
    }

    rv = outcome::success();

    return rv;
}

auto CommandStore::is_argument( Uuid const& node )
    -> bool
{
    auto& km = kmap_inst();
    auto const desc = view::make( node )
                    | view::ancestor( KTRYE( argument_root() ) )
                    | view::exists( km );
    auto const structure = view::make( node )
                         | view::child( view::all_of( "description", "guard", "completion" ) )
                         | view::exists( km );

    return desc && structure;
}

auto CommandStore::is_command( Uuid const& node )
    -> bool
{
    auto& km = kmap_inst();
    auto const desc = view::make( node )
                    | view::ancestor( KTRYE( command_root() ) )
                    | view::exists( km );
    auto const structure = view::make( node )
                         | view::child( view::all_of( "description", "argument", "action" ) )
                         | view::exists( km );

    return desc && structure;
}

auto CommandStore::uninstall_argument( Uuid const& argn )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE_MSG( is_argument( argn ), error_code::common::uncategorized, "attempting to uninstall non-argument" );

    KTRY( nw->erase_node( argn ) );

    rv = outcome::success();

    return rv;
}

auto CommandStore::uninstall_command( Uuid const& cmdn )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_command( cmdn ), error_code::common::uncategorized );

    auto const abspath = KTRY( view::make( km.root_node_id() )
                             | view::desc( cmdn )
                             | act::abs_path_flat( km ) );

    fmt::print( "uninstalling: {}\n", abspath );
    KTRY( nw->erase_node( cmdn ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::com

namespace
{
    namespace command_store_def
    {
        using namespace std::string_literals;

        REGISTER_COMPONENT
        (
            kmap::com::CommandStore
        ,   std::set({ "root_node"s, "network"s })
        ,   "maintains commands"
        );
    } // namespace command_store_def 
} // namespace anonymous
