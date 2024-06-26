/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "node_manip.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../emcc_bindings.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "../utility.hpp"
#include "command.hpp"
#include "com/cmd/command.hpp"
#include "com/network/network.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/view/transform.hpp>

using namespace ranges;

namespace kmap::cmd {

// auto create_child( Kmap& kmap
//                  , Uuid const& parent
//                  , Title const& title
//                  , Heading const& heading )
//     -> Result< std::string >
// {
//     if( !nw->is_child( parent, heading ) )
//     { 
//         auto const nw = KTRY( kmap.fetch_component< com::Network >() );
//         auto const cid = KTRY( nw->create_child( parent, heading ) );

//         KTRY( nw->update_title( cid, title ) );
//         KTRY( kmap.select_node( cid ) );

//         return fmt::format( "added: {}", heading );
//     }
//     else
//     {
//         return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "path already exists" ) );
//     }
// }

auto merge_nodes( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        KM_RESULT_PROLOG();

        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
        auto const heading = args[ 0 ];

        if( nw->exists( heading ) )
        {
            auto const dst = KTRYE( anchor::node( kmap.root_node_id() )
                                  | view2::desc( heading )
                                  | act2::fetch_node( kmap ) );
            auto const src = nw->selected_node();

            merge_trees( *nw, src, dst );

            KTRY( nw->select_node( dst ) );

            return "merge succeeded";
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "merge failed; invalid path" );
        }
    };
}

// auto create_sibling( Kmap& kmap )
//     -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
// {
//     return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
//     {
//         BC_CONTRACT()
//             BC_PRE([ & ]
//             {
//                 BC_ASSERT( args.size() >= 1 );
//             })
//         ;

//         auto const title = flatten( args, ' ' );
//         auto const heading = format_heading( title );
//         auto const target = nw->selected_node();
//         auto const parent = nw->fetch_parent( target );

//         if( parent )
//         {
//             return create_child( kmap
//                                , parent.value()
//                                , title
//                                , heading );
//         }
//         else
//         {
//             BC_ASSERT( target == kmap.root_node_id() );

//             return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "cannot create sibling for root" );
//         }
//     };
// }

#if 0
namespace { 
// TODO: I'm not sure about these to/from. As per the kmap guidelines, the "present" node
//       is always the "source." Thus, copy.body treats the present node as the source,
//       and the provided heading path as the dst. to/from doesn't really elucidate the function.
//       Then again, how does one idiomatically copy _to_ the present node?
namespace copy_body_from_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const src = kmap.fetch_node( args.get( 0 ) );
const dst = kmap.selected_node();

if( src.has_value() )
{
    const body = nw->fetch_body( src.value() );

    nw->update_body( dst, body.value() );
    
    kmap.select_node( dst );

    rv = kmap.success( 'body copied' );
}
else
{
    rv = kmap.failure( dst.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "copies body content from target node";
auto const arguments = std::vector< Argument >{ Argument{ "source_node_path"
                                                        , "source node path"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    copy.body.from
,   description 
,   arguments
,   guard
,   action
);

} // namespace copy_body_from_def

namespace copy_body_to_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const src = kmap.selected_node();
const dst = kmap.fetch_node( args.get( 0 ) );

if( dst.has_value() )
{
    const body = nw->fetch_body( src );

    nw->update_body( dst.value(), body.value() );

    kmap.select_node( dst.value() );

    rv = kmap.success( 'body copied' );
}
else
{
    rv = kmap.failure( dst.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "copies body content to target node";
auto const arguments = std::vector< Argument >{ Argument{ "destination_node_path"
                                                        , "destination node path"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    copy.body.to
,   description 
,   arguments
,   guard
,   action
);

} // namespace copy_body_to_def

namespace create_alias_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const src = kmap.fetch_node( args.get( 0 ) );

if( src.has_value() )
{
    const c = kmap.create_alias( src.value(), nw->selected_node() );

    if( c.has_value() )
    {
        kmap.select_node( nw->selected_node() ); // Refresh.

        rv = kmap.success( 'alias created' );
    }
    else
    {
        rv = kmap.failure( c.error_message() );
    }
}
else
{
    rv = kmap.failure( src.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "creates alias node as a child of selected node";
auto const arguments = std::vector< Argument >{ Argument{ "source_node"
                                                        , "heading path of source node"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.alias
,   description 
,   arguments
,   guard
,   action
);

SCENARIO( "create.alias", "[cmd][create.alias]" )
{
    GIVEN( "commands initialized" )
    {
        KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

        auto& kmap = Singleton::instance();
        auto const nw= REQUIRE_TRY( kmap.fetch_component< com::Network >() );
        auto const root = kmap.root_node_id();

        REQUIRE( nw->exists( "/meta.setting.command.create.alias" ) );

        GIVEN( "nodes: /1.2, /2.1.2" )
        {
            REQUIRE_RES( view::make( root ) 
                       | view::direct_desc( "1.2" )
                       | view::create_node( kmap ) );
            REQUIRE_RES( view::make( root ) 
                       | view::direct_desc( "2.1.2" )
                       | view::create_node( kmap ) );

            GIVEN( "select_node( '/1.2' )" )
            {
                // TODO: Can I figure out a way to attach "rv" to "ensure", KTRY? Such that meta-info could be transmitted along with return.
                //       It's desirable, but tough. Without manually attaching "rv" to each call, and having a separate macro.
                //       I could do something round about and have rv push to a global, thread-safe, singleton, it's meta info, that ensure/try pulls in...
                //       But maybe going through the effort to import rv meta info isn't the right way anyway... which would mean placing meta info in rv in the first place
                //       would only make sense in the limited circumstances where rv is returned without assignment.
                REQUIRE_RES( kmap.select_node( "/1.2" ) );

                WHEN( "create.alias /2.1.2" )
                {
                    auto const cli = REQUIRE_TRY( kmap.fetch_component< com::Cli >() );
                    REQUIRE_RES( cli->parse_raw( ":create.alias /2.1.2" ) );

                    THEN( "alias /1.2.2[/2.1.2] exists" )
                    {
                        auto const dst = REQUIRE_TRY( kmap.fetch_descendant( "/1.2.2" ) );
                        auto const src = REQUIRE_TRY( kmap.fetch_descendant( "/2.1.2" ) );

                        REQUIRE( nw->resolve( dst ) == src );
                    }
                }
                WHEN( "create.alias 2'1'2" )
                {
                    auto const cli = REQUIRE_TRY( kmap.fetch_component< com::Cli >() );
                    REQUIRE_RES( cli->parse_raw( ":create.alias 2'1'2" ) );
                }
            }
        }
    }
}

} // namespace create_alias_def

namespace create_child_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const c = kmap.create_child( nw->selected_node(), args.get( 0 ) );

if( c.has_value() )
{
    kmap.select_node( c.value() );

    rv = kmap.success( 'node created' );
}
else
{
    rv = kmap.failure( c.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "creates node as a child of selected node";
auto const arguments = std::vector< Argument >{ Argument{ "child_title"
                                                        , "title for child node"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional" , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.child
,   description 
,   arguments
,   guard
,   action
);

} // namespace create_child_def

namespace create_reference_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const src = kmap.fetch_node( args.get( 0 ) );
const sel = nw->selected_node();
const res = kmap.create_reference( src.value(), sel );

if( res.has_value() )
{
    kmap.select_node( sel );

    rv = kmap.success( 'reference created' );
}
else
{
    rv = kmap.failure( res.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "creates reference child and an alias to the source node";
auto const arguments = std::vector< Argument >{ Argument{ "source_node"
                                                        , "heading path of source node"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.reference
,   description 
,   arguments
,   guard
,   action
);

} // namespace create_reference_def

namespace create_sibling_def {

auto const guard_code =
R"%%%(```javascript
if( kmap.root_node() == nw->selected_node() )
{
    return kmap.failure( 'root cannot have a sibling' );
}
else
{
    return kmap.success( 'not root' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const parent = nw->fetch_parent( nw->selected_node() );
const c = kmap.create_child( parent.value(), args.get( 0 ) );

if( c.has_value() )
{
    kmap.select_node( c.value() );

    rv = kmap.success( 'node created' );
}
else
{
    rv = kmap.failure( c.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "creates node as a sibling of selected node";
auto const arguments = std::vector< Argument >{ Argument{ "sibling_title"
                                                        , "sibling node title"
                                                        , "unconditional" } };
auto const guard = Guard{ "nonroot", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.sibling
,   description 
,   arguments
,   guard
,   action
);

} // namespace create_child_def

namespace delete_children_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.success( 'no children' );
const selected = nw->selected_node();
const children = nw->fetch_children( selected );

rv = kmap.delete_children( selected );

if( rv.has_value() )
{
    kmap.select_node( selected );

    rv = kmap.success( 'children deleted' );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "deletes child nodes of selected";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    delete.children
,   description 
,   arguments
,   guard
,   action
);

} // namespace delete_children_def

namespace delete_alias_def {

auto const guard_code =
R"%%%(```javascript
if( kmap.network().is_alias( nw->selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not an alias' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
res = kmap.delete_alias( nw->selected_node() );

if( res.has_value() )
{
    kmap.select_node( res.value() );

    rv = kmap.success( 'node deleted' );
}
else
{
    rv = kmap.failure( res.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "deletes selected node";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "deletable"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    delete.alias
,   description 
,   arguments
,   guard
,   action
);

} // namespace delete_alias_def

namespace delete_node_def {

auto const guard_code =
R"%%%(```javascript
if( kmap.root_node() === nw->selected_node() )
{
    return kmap.failure( 'cannot delete root node' );
}
else
{
    return kmap.success( 'success' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
res = kmap.delete_node( nw->selected_node() );

if( res.has_value() )
{
    kmap.select_node( res.value() );

    rv = kmap.success( 'node deleted' );
}
else
{
    rv = kmap.failure( res.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "deletes selected node";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "deletable"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    delete.node
,   description 
,   arguments
,   guard
,   action
);

namespace delete_def
{
    // TODO: Rather than REGISTER_COMMAND, create a new macro, REGISTER_COMMAND_ALIAS
    //       This will alias delete.node, for "delete". It will create a dependeny on REGISTER_COMMAND,
    //       so it will need to be called after all commands have been registered.
    //       Another thought that occurred to me is that "detectable" then becomes a potential conflict.
    //       To avoid such a potential conflict, guards could be very specifically named
    //       e.g. guard_context_deletable. It is highly unlikely that such a name would conflict with future
    //       commands.
    //       Example:
    // REGISTER_COMMAND_ALIAS
    // (
    //     delete
    // ,   delete.node.deletable
    // );
    // REGISTER_COMMAND
    // (
    //     delete
    // ,   description 
    // ,   arguments
    // ,   guard
    // ,   action
    // );
} // delete_def

} // namespace delete_node_def

namespace move_bottom_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const sel = nw->selected_node();
const below = kmap.fetch_below( sel );
const pos = kmap.fetch_sibling_position( sel ).value();
const size = nw->fetch_children_ordered( nw->fetch_parent( sel ).value() ).value();
// err.. not sure how I want the API to look here... Could be as simple as reorder_children( p, rearranged );

while( below.has_value() )
{
    let swapped = nw->swap_nodes( sel, above.value() );

    if( swapped.has_value() )
    {
        kmap.select_node( sel );

        rv = kmap.success( "node moved up" );
    }
    else
    {
        rv = kmap.failure( swapped.error_message() );
    }
}
else
{
    rv = kmap.failure( above.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "repositions selected node at bottom of children";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    move.bottom
,   description 
,   arguments
,   guard
,   action
);

} // namespace move_up_def

namespace move_body_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const src = nw->selected_node();
const dst = kmap.fetch_node( args.get( 0 ) );

if( dst.has_value() )
{
    if( nw->move_body( src, dst.value() ) )
    {
        kmap.select_node( dst.value() );

        rv = kmap.success( 'move body succeeded' );
    }
    else
    {
        rv = kmap.failure( 'move body failed' );
    }
}
else
{
    rv = kmap.failure( dst.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "moves selected node's body to destination node";
auto const arguments = std::vector< Argument >{ Argument{ "target_node"
                                                        , "Destination node for selected node body"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    move.body
,   description 
,   arguments
,   guard
,   action
);

} // namespace move_body_def

namespace move_children_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.success( "children moved" );
const src = nw->selected_node();
const dst = kmap.fetch_node( args.get( 0 ) );

if( dst.has_value() )
{
    const cs = nw->fetch_children( src );

    for( let i = 0
       ; i < cs.size()
       ; ++i )
    {
        const c = cs.get( i );
        const moved = kmap.move_node( c, dst.value() );

        if( moved.has_error() )
        {
            rv = kmap.failure( moved.error_message() );
            break;
        }
    }

    if( !rv.has_error() )
    {
        kmap.select_node( dst.value() );
    }
}
else
{
    rv = kmap.failure( dst.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "moves selected node to destination";
auto const arguments = std::vector< Argument >{ Argument{ "target_node"
                                                        , "Destination node for selected node"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    move.children
,   description 
,   arguments
,   guard
,   action
);

} // namespace move_children_def

namespace move_down_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const sel = kmap.selected_node();
const below = kmap.fetch_below( sel );

if( below.has_value() )
{
    const swapped = kmap.swap_nodes( sel, below.value() );

    if( swapped.has_value() )
    {
        kmap.select_node( sel );

        rv = kmap.success( "node moved up" );
    }
    else
    {
        rv = kmap.failure( swapped.error_message() );
    }
}
else
{
    rv = kmap.failure( below.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "swaps selected node with below node";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    move.down
,   description 
,   arguments
,   guard
,   action
);

} // namespace move_down_def

namespace move_node_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const src = kmap.selected_node();
const dst = kmap.fetch_node( args.get( 0 ) );

if( dst.has_value() )
{
    const jump_out_dst = ( () =>
    {
        let r = kmap.fetch_above( src );
        if( r.has_value() )
        {
            return r.value();
        }
        r = kmap.fetch_below( src );
        if( r.has_value() )
        {
            return r.value();
        }
        else
        {
            return kmap.fetch_parent( src ).value();
        }
    } )();
    const c = kmap.move_node( src, dst.value() );

    if( c.has_value() )
    {
        kmap.select_node( src );
        kmap.jump_stack().jump_in( jump_out_dst );

        rv = kmap.success( 'node moved' );
    }
    else
    {
        rv = kmap.failure( c.error_message() );
    }
}
else
{
    rv = kmap.failure( dst.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "moves selected node to destination";
auto const arguments = std::vector< Argument >{ Argument{ "target_node"
                                                        , "Destination node for selected node"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    move.node
,   description 
,   arguments
,   guard
,   action
);

} // namespace move_node_def

namespace move_up_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const sel = kmap.selected_node();
const above = kmap.fetch_above( sel );

if( above.has_value() )
{
    const swapped = kmap.swap_nodes( sel, above.value() );

    if( swapped.has_value() )
    {
        kmap.select_node( sel );

        rv = kmap.success( "node moved up" );
    }
    else
    {
        rv = kmap.failure( swapped.error_message() );
    }
}
else
{
    rv = kmap.failure( above.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "swaps selected node with above node";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    move.up
,   description 
,   arguments
,   guard
,   action
);

} // namespace move_up_def

namespace sort_children_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();

rv = kmap.sort_children( selected );

kmap.select_node( selected ); // Refresh graph.

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "exchanges given node with selected";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    sort.children
,   description 
,   arguments
,   guard
,   action
);

} // namespace sort_children_def

namespace swap_node_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const lhs = kmap.selected_node();
const rhs = kmap.fetch_node( args.get( 0 ) );

if( rhs.has_value() )
{
    const c = kmap.swap_nodes( lhs, rhs.value() );

    if( c.has_value() )
    {
        kmap.select_node( lhs );

        rv = kmap.success( 'nodes swapped' );
    }
    else
    {
        rv = kmap.failure( c.error_message() );
    }
}
else
{
    rv = kmap.failure( rhs.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "exchanges given node with selected";
auto const arguments = std::vector< Argument >{ Argument{ "path_to_target_node"
                                                        , "path to target node"
                                                        , "heading_path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    swap.node
,   description 
,   arguments
,   guard
,   action
);

} // namespace swap_node_def

namespace update_body_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();

rv = kmap.update_body( selected, args.get( 0 ) );

if( rv.has_value() )
{
    kmap.select_node( selected );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "overwrites existing body with given one";
auto const arguments = std::vector< Argument >{ Argument{ "body_content"
                                                        , "replacement body content"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    update.body
,   description 
,   arguments
,   guard
,   action
);

} // namespace update_body_def

namespace update_heading_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();

rv = kmap.update_heading( selected, args.get( 0 ) );

if( rv.has_value() )
{
    kmap.select_node( selected );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "overwrites existing heading with given one";
auto const arguments = std::vector< Argument >{ Argument{ "heading"
                                                        , "replacement heading"
                                                        , "heading" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    update.heading
,   description 
,   arguments
,   guard
,   action
);

} // namespace update_heading_def

namespace update_title_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();

rv = kmap.update_title( selected, args.get( 0 ) );

if( rv.has_value() )
{
    kmap.select_node( selected );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "overwrites existing title with given one";
auto const arguments = std::vector< Argument >{ Argument{ "title"
                                                        , "replacement title"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    update.title
,   description 
,   arguments
,   guard
,   action
);

} // namespace update_title_def

} // namespace anon
#endif // 0

namespace binding {

using namespace emscripten;

auto create_reference( Uuid const& src
                     , Uuid const& dst )
    -> kmap::Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "src", src );
        KM_RESULT_PUSH_NODE( "dst", dst );

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto const ref = KTRY( anchor::node( dst )
                         | view2::child( "references" )
                         | act2::fetch_or_create_node( km )
                         | act2::single );

    return nw->create_alias( src, ref );
}

EMSCRIPTEN_BINDINGS( kmap_node_manip )
{
    function( "create_reference", &kmap::cmd::binding::create_reference );
}

} // namespace binding::anon

} // namespace kmap::cmd