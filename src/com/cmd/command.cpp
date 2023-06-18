/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/cmd/command.hpp>

#include "com/network/network.hpp"
#include "error/js_iface.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/abs_path.hpp"
#include "path/act/back.hpp"
#include "path/act/erase.hpp"
#include "path/act/update_body.hpp"
#include "path/node_view.hpp"
#include "path.hpp"
#include "util/script/script.hpp"
#include "util/result.hpp"

namespace kmap::com {

auto CommandStore::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( install_standard_arguments() );
    KTRY( install_standard_guards() );

    rv = outcome::success();

    return rv;
}

auto CommandStore::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    rv = outcome::success();

    return rv;
}

auto CommandStore::install_argument( Argument const& arg )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "arg", arg.path );

    auto rv = result::make_result< Uuid >();
    auto& km = kmap_inst();

    KMAP_ENSURE( js::lint( arg.guard ), error_code::js::lint_failed );
    KMAP_ENSURE( js::lint( arg.completion ), error_code::js::lint_failed );
    
    auto const arg_root = KTRY( view2::cmd::argument_root
                              | act2::fetch_or_create_node( km ) );
    auto const vargn = view::make( arg_root ) 
                     | view::direct_desc( arg.path );
    auto const guard_code = util::to_js_body_code( js::beautify( arg.guard ) );
    auto const completion_code = util::to_js_body_code( js::beautify( arg.completion ) );
    
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
        KTRY( vargn | view::child( "guard" ) | act::update_body( km, guard_code ) );
        KTRY( vargn | view::child( "completion" ) | act::update_body( km, completion_code ) );

        rv = KTRY( vargn | view::fetch_node( km ) );
    }

    return rv;
}

auto CommandStore::install_command( Command const& cmd )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", cmd.path );

    auto rv = result::make_result< Uuid >();

    KMAP_ENSURE( js::lint( cmd.action ), error_code::js::lint_failed );

    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const cmd_root = KTRY( view2::cmd::command_root
                              | act2::fetch_or_create_node( km ) );
    auto const vcmd = view::make( cmd_root )
                    | view::direct_desc( cmd.path );

    KTRY( vcmd | view::create_node( km ) );

    auto const cmdn = KTRY( vcmd | view::fetch_node( km ) );

    KTRY( vcmd
        | view::child( view::all_of( "description"
                                   , "argument"
                                   , "guard"
                                   , "action" ) )
        | view::create_node( km ) );

    // desc
    {
        auto const descn = KTRY( vcmd | view::child( "description" ) | view::fetch_node( km ) );
        
        KTRY( nw->update_body( descn, cmd.description ) );
    }

    // guard
    {
        auto const guardn = KTRY( vcmd | view::child( "guard" ) | view::fetch_node( km ) );
        auto const guard_src = KTRY( view2::cmd::guard_root
                                   | view2::direct_desc( cmd.guard )
                                   | act2::fetch_node( km ) );
        KTRY( nw->create_alias( guard_src, guardn ) );
    }

    // arg
    for( auto const& arg : cmd.arguments )
    {
        auto const argn = KTRY( vcmd | view::child( "argument" ) | view::fetch_node( km ) );
        auto const arg_src = KTRY( view2::cmd::argument_root
                                 | view2::direct_desc( arg.argument_alias)
                                 | act2::fetch_node( km ) );
        auto const arg_dst = KTRY( nw->create_child( argn, arg.heading ) );
        auto const nw = KTRY( fetch_component< com::Network >() );

        KTRY( nw->update_body( arg_dst, arg.description ) );
        KTRY( nw->create_alias( arg_src, arg_dst ) );
    }

    // action
    {
        auto const actn = KTRY( vcmd
                            | view::child( "action" )
                            | view::fetch_node( km ) );
        auto const action_code = util::to_js_body_code( js::beautify( cmd.action ) );

        KTRY( nw->update_body( actn, action_code ) );
    }

    rv = cmdn;

    return rv;
}

auto CommandStore::install_guard( Guard const& guard )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", guard.path );

    auto rv = result::make_result< Uuid >();
    auto& km = kmap_inst();

    KMAP_ENSURE( js::lint( guard.action ), error_code::js::lint_failed );
    
    auto const guard_root = KTRY( view2::cmd::guard_root
                                | act2::fetch_or_create_node( km ) );
    auto const vguardn = view::make( guard_root ) 
                       | view::direct_desc( guard.path );
    auto const guard_code = util::to_js_body_code( js::beautify( guard.action ) );
    
    if( vguardn | view::exists( km ) )
    {
        KTRY( vguardn | act::erase( km ) );
    } 

    {
        KTRY( vguardn
            | view::child( view::all_of( "description", "action" ) )
            | view::create_node( km ) );
        
        KTRY( vguardn | view::child( "description" ) | act::update_body( km, guard.description ) );
        KTRY( vguardn | view::child( "action" ) | act::update_body( km, guard_code ) );

        rv = KTRY( vguardn | view::fetch_node( km ) );
    }

    return rv;
}

// TODO: Should this be a separate component? E.g., "cmd.standard_arguments"?
auto CommandStore::install_standard_arguments()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // arg.unconditional
    {
        auto const guard_code =
        R"%%%(
            /* Nothing to do. */
        )%%%";
        auto const completion_code =
        R"%%%(
            let rv = new kmap.VectorString();

            rv.push_back( arg );

            return rv;
        )%%%";

        auto const description = "any valid text";

        KTRY( install_argument( Argument{ .path = "unconditional"
                                        , .description = description
                                        , .guard = guard_code
                                        , .completion = completion_code } ) );
    }
    // arg.filesystem_path // TODO: I suspect that this belongs in a "filesystem" component.
    {
        auto const guard_code =
        R"%%%(
            /* Nothing to do. */
        )%%%";
        auto const completion_code =
        R"%%%(
            return kmap.complete_filesystem_path( arg );
        )%%%";

        auto const description = "filesystem path";

        KTRY( install_argument( Argument{ .path = "filesystem_path"
                                        , .description = description
                                        , .guard = guard_code
                                        , .completion = completion_code } ) );
    }
    // arg.heading_path
    {
        auto const guard_code =
        R"%%%(
            /* Nothing to do. */
        )%%%";
        auto const completion_code =
        R"%%%(
            return kmap.complete_heading_path( arg );
        )%%%";

        auto const description = "heading path";

        KTRY( install_argument( Argument{ .path = "heading_path"
                                        , .description = description
                                        , .guard = guard_code
                                        , .completion = completion_code } ) );
    }

    rv = outcome::success();

    return rv;
}

auto CommandStore::install_standard_guards()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // unconditional
    {
        auto const guard_code =
        R"%%%(
            /* Nothing to do. */
        )%%%";

        auto const description = "without environmental constraints";

        KTRY( install_guard( Guard{ .path = "unconditional"
                                  , .description = description
                                  , .action = guard_code } ) );
    }

    rv = outcome::success();

    return rv;
}

auto CommandStore::is_argument( Uuid const& node )
    -> bool
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto& km = kmap_inst();
    auto const desc = view2::cmd::argument_root
                    | act2::exists( km );
    auto const structure = view::make( node )
                         | view::child( view::all_of( "description", "guard", "completion" ) )
                         | view::exists( km );

    return desc && structure;
}

auto CommandStore::is_command( Uuid const& node )
    -> bool
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto& km = kmap_inst();
    auto const desc = view2::cmd::command_root
                    | act2::exists( km );
    auto const structure = view::make( node )
                         | view::child( view::all_of( "description", "argument", "action" ) )
                         | view::exists( km );

    return desc && structure;
}

auto CommandStore::is_guard( Uuid const& node )
    -> bool
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto& km = kmap_inst();
    auto const desc = view2::cmd::guard_root
                    | act2::exists( km );
    auto const structure = view::make( node )
                         | view::child( view::all_of( "description", "action" ) )
                         | view::exists( km );

    return desc && structure;
}

auto CommandStore::uninstall_argument( Uuid const& argn )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "arg", argn );

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
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "cmdn", cmdn );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_command( cmdn ), error_code::common::uncategorized );

    // auto const abspath = KTRY( view::abs_root
    auto const abspath = KTRY( view::make( km.root_node_id() )
                             | view::desc( cmdn )
                             | act::abs_path_flat( km ) );

    fmt::print( "uninstalling: {}\n", abspath );
    KTRY( nw->erase_node( cmdn ) );

    rv = outcome::success();

    return rv;
}

auto CommandStore::uninstall_guard( Uuid const& guardn )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "guardn", guardn );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE( is_guard( guardn ), error_code::common::uncategorized );

    // auto const abspath = KTRY( view::abs_root
    auto const abspath = KTRY( view::make( km.root_node_id() )
                             | view::desc( guardn )
                             | act::abs_path_flat( km ) );

    KTRY( nw->erase_node( guardn ) );

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
