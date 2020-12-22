/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cmd.hpp"

#include "arg.hpp"
// #include "cmd/bookmark.hpp"
#include "cmd/call_map.hpp"
#include "cmd/cardinality.hpp"
#include "cmd/command.hpp"
#include "cmd/conclusion.hpp"
#include "cmd/definition.hpp"
#include "cmd/dotlang.hpp"
// #include "cmd/echo.hpp"
#include "cmd/log.hpp"
#include "cmd/next_fork.hpp"
#include "cmd/node_manip.hpp"
#include "cmd/project.hpp"
#include "cmd/recipe.hpp"
// #include "cmd/repair.hpp"
#include "cmd/resource.hpp"
// #include "cmd/script.hpp"
#include "cmd/search.hpp"
#include "cmd/select_node.hpp"
#include "cmd/state.hpp"
#include "cmd/tag.hpp"
#include "cmd/text.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "test/master.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <range/v3/action/join.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/intersperse.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/replace.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>
#include <sqlpp11/insert.h>

#include <vector>
#include <chrono>

using namespace ranges;

namespace kmap::cmd { namespace {

// TODO: These helper functions belong in utility.cpp, or, if applicable, Kmap:://conveniences.

template< size_t Index >
auto fetch_leaf( CliCommand::Args const& args
               , Kmap const& kmap )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( args.size() >= Index + 1 )
    {
        auto const& p = kmap.fetch_descendants( args[ Index ]  );
        
        rv = p ? p.value().back() : Optional< Uuid >{}; // TODO: This is clearly wrong. Simply selected the "last" possible path returned is not good practice!
    }

    return rv;
}

template< size_t Index >
auto fetch_leaf_or_selected( CliCommand::Args const& args
                           , Kmap const& kmap )
    -> Optional< Uuid >
{
    auto rv = fetch_leaf< Index >( args
                                 , kmap );
    if( !rv
     && args.size() < Index + 1 )
    {
        rv = kmap.selected_node();
    }

    return rv;
}

auto absolute_path( Kmap& kmap )
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() <= 1 );
            })
        ;

        auto const& target = fetch_leaf_or_selected< 0 >( args
                                                        , kmap );

        if( !target )
        {
            return { CliResultCode::failure
                   , fmt::format( "Target node not found: {}"
                                , args[ 0 ] ) };
        }

        auto const ap = kmap.absolute_path_flat( *target );

        return { CliResultCode::success
               , ap };
    };
}

auto run_unit_tests( Kmap& kmap )
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        (void)kmap;
        (void)args;

        fmt::print( "[Notice] Unloading Current and Loading Testing Environment.\n" );

        auto output = kmap::run_unit_tests( /*StringVec{}*/ );

        if( output == 0 )
        {
             return { CliResultCode::success
                    , fmt::format( "unit test result: {}"
                                 , output ) };
        }
        else
        {
             return { CliResultCode::failure
                    , fmt::format( "unit test result: {}"
                                 , output ) };
        }
    };
}

auto list_commands( Kmap& kmap )
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        (void)args;
        
        auto filter_key = []( auto const& c )
        {
            return c.command;
        };
        auto const cmds = kmap.cli()
                              .valid_commands();
        auto const keys = cmds
                        | views::transform( filter_key )
                        | to_vector
                        | actions::sort;
        auto flat = keys
                  | views::join( '\n' )
                  | to< std::string >();
        auto& tv = kmap.text_view();

        tv.clear();
        tv.show_preview( flat );
        kmap.network()
            .focus();

        return { CliResultCode::success
               , "list-command" };
    };
}

auto help( Kmap& kmap )
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.empty() || args.size() == 1 );
            })
        ;

        auto const linebreak = "\n\n"; // Markdown doesn't treat single '\n' as a line break.
        auto const out_text = [ & ]() -> std::string
        {
            auto format_help_msg = []( auto const& c )
            {
                auto const help_msg = [ & ]
                {
                    auto rv = c.help;

                    if( !rv.empty() )
                    {
                        rv[ 0 ] = std::toupper( rv[ 0 ] );
                    }

                    return rv;
                }();
                return fmt::format( "{}."
                                  , help_msg );
            };
            auto format_no_args = [ & ]( auto const& c )
            {
                return fmt::format( "{} | {}"
                                  , c.command
                                  , format_help_msg( c ) );
            };

            if( args.empty() )
            {
                auto const cmds = kmap.cli()
                                      .valid_commands();
                auto const ts = cmds
                              | views::transform( format_no_args )
                              | to_vector;
                auto const flat = ts
                                | views::intersperse( linebreak )
                                | actions::join
                                | to< std::string >();

                return flat;
            }
            else
            {
                auto const cmds = kmap.cli()
                                      .valid_commands();
                auto const cmd_it = find_if( cmds
                                           , [ & ]( auto const& e ){ return e.command == args[ 0 ]; } );

                // TODO: Handle fail case (need to return failure with
                // unrecognized command? Or Should CliCommand->is_malformed
                // ensure if we get here it's guaranteed to be a valid command.
                if( cmd_it == end( cmds ) )
                {
                    return "command not found";
                }

                // TODO: Does not properly handle descriptions (arg->desc()) with spaces.
                auto const arg_help = [ & ]
                {
                    auto v = std::vector< std::string >{};
                    auto cmd = cmd_it->command;

                    for( auto const& arg : cmd_it->args )
                    {
                        cmd = fmt::format( "{} {}"
                                         , cmd
                                         , '[' + arg->desc() + ']' ); // TODO: Why can't I use <> here instead?
                        v.emplace_back( fmt::format( "{} | {}"
                                                   , cmd
                                                   , arg->cmd_ctx_desc() ) );
                    }

                    return v
                         | views::intersperse( linebreak )
                         | actions::join
                         | to< std::string >();
                }();

                return fmt::format( "{}{}{}"
                                  , format_no_args( *cmd_it )
                                  , linebreak
                                  , arg_help );
            }
        }();

        auto& tv = kmap.text_view();

        tv.clear();
        tv.hide_editor();
        tv.show_preview( markdown_to_html( out_text ) );

        return { CliResultCode::success
               , "See preview" };
    };
}

auto jump_in( Kmap& kmap )
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.empty() || args.size() == 1 );
            })
        ;

        auto& js = kmap.jump_stack(); 
        auto const rv = [ & ] // TODO: Try to reuse kmap::jump_out
        {
            // auto nid = js.jump_in();

            // BC_ASSERT( nid == kmap.selected_node() );

            // (void)nid;

            return js.jump_in();
        }();

        if( !rv )
        {
            return { CliResultCode::failure
                   , fmt::format( "jump stack empty" ) };
        }
        else
        {
            kmap.jump_to( *rv );

            return { CliResultCode::success
                   , fmt::format( "{} entered"
                                , kmap.fetch_title( *rv ).value() ) };
        }
    };
}

auto jump_out( Kmap& kmap )
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.empty() || args.size() == 1 );
            })
        ;

        auto& js = kmap.jump_stack(); 
        auto const rv = [ & ] // TODO: Try to reuse kmap::jump_out
        {
            // auto nid = js.jump_out();

            // BC_ASSERT( nid == kmap.selected_node() );

            // (void)nid;

            return js.jump_out();
        }();

        if( !rv )
        {
            return { CliResultCode::failure
                   , fmt::format( "jump stack empty" ) };
        }
        else
        {
            kmap.select_node( *rv );

            return { CliResultCode::success
                   , fmt::format( "{} entered"
                                , kmap.fetch_title( *rv ).value() ) };
        }
    };
}

auto edit_title( Kmap& kmap )
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.empty() || args.size() == 1 );
            })
        ;

        auto const& target = fetch_leaf_or_selected< 0 >( args
                                                        , kmap );

        if( !target )
        {
            return { CliResultCode::failure
                   , fmt::format( "target node not found" ) };
        }
        else
        {
            auto& cli = kmap.cli();

            cli.clear_input();
            cli.show_popup( "enter title" );

            auto const text = cli.get_input();

            // TODO: this needs to ensure that any aliases dependent on this
            // get dynamically updated as well. The body isn't an issue, as the
            // preview will resolve the alias to the original/real node.
            kmap.update_title( *target
                             , text );

            return { CliResultCode::success
                   , "node title open for editing" };
        }
    };
}

// TODO: Test corner cases e.g., an alias aliasing itself, recursive aliases trees, etc.
// auto create_alias( Kmap& kmap )
// {
//     return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
//     {
//         BC_CONTRACT()
//             BC_PRE([ & ]
//             {
//                 BC_ASSERT( args.size() == 1 || args.size() == 2 );
//             })
//         ;

//         auto const& source = fetch_leaf< 0 >( args
//                                             , kmap );

//         if( !source )
//         {
//             return { CliResultCode::failure
//                    , fmt::format( "Source node not found: {}"
//                                 , args[ 0 ] ) };
//         }

//         auto const& target = fetch_leaf_or_selected< 1 >( args
//                                                         , kmap );

//         if( !target )
//         {
//             return { CliResultCode::failure
//                    , fmt::format( "Target node not found: {}"
//                                 , args[ 0 ] ) };
//         }

//         if( auto const alias = kmap.create_alias( *source
//                                                 , *target )
//           ; alias )
//         {
//             kmap.jump_to( alias.value() );

//             return { CliResultCode::success
//                    , "alias added" };
//         }
//         else
//         {
//             return { CliResultCode::failure
//                    , "failed to create alias" };
//         }
//     };
// }

// TODO: Place in cmd/select_node.cpp?
// auto resolve_alias( Kmap& kmap )
// {
//     return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
//     {
//         BC_CONTRACT()
//             BC_PRE([ & ]
//             {
//                 BC_ASSERT( args.empty() || args.size() == 1 );
//             })
//         ;

//         auto const& target = fetch_leaf_or_selected< 0 >( args
//                                                         , kmap );

//         if( !target )
//         {
//             return { CliResultCode::failure
//                    , fmt::format( "target node not found" ) };
//         }
//         else
//         {
//             kmap.select_node( kmap.resolve( *target ) );

//             return { CliResultCode::success
//                    , "resolved" };
//         }
//     };
// }

} // anonymous ns

auto make_core_commands( Kmap& kmap )
    -> std::vector< CliCommand >
{
    return std::vector< CliCommand >
    {
        { "absolute.path"
        , "displays full node path"
        , ArgumentList{}
            | HeadingPathArg{ "node path"
                            , "displays path for <_>" 
                            , kmap }
                | Attr::optional
        , absolute_path( kmap )
        }
    ,   { "activate.project" 
        , "places selected project in open and activated status"
        , ArgumentList{}
            | ProjectArg{ "heading" 
                        , "activates <_>"
                        , kmap }
                | Attr::optional
        , activate_project( kmap )
        }
    // ,   { "add.daily.task"
    //     , "adds ancestral project to selected node as a alias task of the present daily log"
    //     , ArgumentList{}
    //     , add_daily_task( kmap )
    //     }
    ,   { "add.definition"
        , "associates node with given definition"
        , ArgumentList{}
             | InvertedPathArg{ "inverted definition path"
                             , "associates <_> with selected node" // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
                             , "/definitions"
                             , kmap }
        , add_definition( kmap )
        }
    ,   { "add.objection" 
        , "adds a reference to an existing conclusion, as a objection of the present conclusion"
        , ArgumentList{}
            | ConclusionArg{ "heading" 
                           , "creates a reference to an existing objection, <_>, for the present conclusion"
                           , kmap }
        , add_objection( kmap )
        }
    ,   { "add.premise" 
        , "adds a reference to an existing conclusion, as a premise of the present conclusion"
        , ArgumentList{}
            | ConclusionArg{ "heading" 
                           , "creates a reference to an existing premise, <_>, for the present conclusion"
                           , kmap }
        , add_premise( kmap )
        }
    ,   { "add.prerequisite" 
        , "adds a reference to an existing recipe, as a prerequisite of the present project"
        , ArgumentList{}
            | RecipeArg{ "heading" 
                       , "creates a reference to an existing prerequisite, <_>, for the present recipe"
                       , kmap }
        , add_prerequisite( kmap )
        }
    ,   { "add.resource"
        , "creates a resource reference for selected node"
        , ArgumentList{}
            | ResourcePathArg{ "resource node path"
                             , "creates a reference to <_>"
                             , kmap }
            | HeadingPathArg{ "target path"
                            , "creates a reference to resource <0> for <_>"
                            , kmap }
                | Attr::optional
        , add_resource( kmap )
        }
    ,   { "add.step" 
        , "adds a reference to an existing recipe, as a step of the present project"
        , ArgumentList{}
            | RecipeArg{ "heading" 
                       , "creates a reference to an existing step, <_>, for the present recipe"
                       , kmap }
        , add_step( kmap )
        }
    ,   { "add.tag"
        , "associates node with given tag"
        , ArgumentList{}
             | InvertedPathArg{ "inverted tag path"
                             , "associates <_> with selected node" // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
                             , "/tags"
                             , kmap }
        , add_tag( kmap )
        }
    // ,   { "add.task" 
    //     , "adds a reference to an existing project, as a task of the present project"
    //     , ArgumentList{}
    //         | ProjectArg{ "heading" 
    //                     , "creates a reference to an existing project, <_> for the present project"
    //                     , kmap }
    //     , add_task( kmap )
    //     }
    // ,   { "create.alias"
    //     , "creates an alias node"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "source node path"
    //                         , "creates an alias node referring to <_> as a child of present node" // TODO: replace "<_>" with user input.
    //                         , kmap }
    //         | HeadingPathArg{ "target node path"
    //                         , "creates an alias node referring to <0> as a child of <_>" // TODO: replace "<_>" with user input.
    //                         , kmap }
    //             | Attr::optional
    //     , create_alias( kmap )
    //     }
    // ,   { "create.bookmark"
    //     , "bookmarks selected node"
    //     , ArgumentList{}
    //     , create_bookmark( kmap )
    //     }
    // ,   { "create.child"
    //     , "appends child to selected node"
    //     , ArgumentList{}
    //         | TitleArg{ "child title"
    //                   , "appends child to selected node" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
    //     , create_child( kmap )
    //     }
    ,   { "create.citation"
        , "creates a reference to a conclusion"
        , ArgumentList{}
            | HeadingPathArg{ "target path"
                            , "creates a reference to <0> for <_>"
                            , kmap }
                | Attr::optional
        , create_citation( kmap )
        }
    // ,   { "create.command"
    //     , "creates a new command node"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "command title"
    //                         , "creates a new command node"
    //                         , kmap }
    //     , create_command( kmap )
    //     }
    ,   { "create.conclusion"
        , "creates a new conclusion node"
        , ArgumentList{}
            | ConclusionArg{ "title" 
                           , "creates a new conclusion, with title: <_>"
                           , kmap }
        , create_conclusion( kmap )
        }
    // ,   { "create.daily.log"
    //     , "creates a log for today's date"
        // , ArgumentList{}
        //     // | DailyLogArg{ "date"
        //     //              , "creates a log event for the date of <_>"
        //     //              , kmap }
        //     //     | Attr::optional
        // , create_daily_log( kmap )
        // }
    ,   { "create.definition"
        , "creates a new definition node"
        , ArgumentList{}
            | TitleArg{ "definition title"
                      , "creates definition <_>" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
        , create_definition( kmap )
        }
    ,   { "create.objection" 
        , "creates a new conclusion, as a objection of the present conclusion"
        , ArgumentList{}
            | TitleArg{ "title" 
                      , "creates a new conclusion, <_>, as a premise of the present conclusion" }
        , create_objection( kmap )
        }
    ,   { "create.premise" 
        , "creates a new conclusion, as a premise of the present conclusion"
        , ArgumentList{}
            | TitleArg{ "title" 
                      , "creates a new conclusion, <_>, as a premise of the present conclusion" }
        , create_premise( kmap )
        }
    ,   { "create.prerequisite" 
        , "creates a new recipe, and adds it as a prerequisite of the present project"
        , ArgumentList{}
            | RecipeArg{ "heading" 
                       , "creates a reference to a new prerequisite, <_>, for the present recipe"
                       , kmap }
        , create_prerequisite( kmap )
        }
    // ,   { "create.project"
    //     , "creates a new project"
    //     , ArgumentList{}
    //         | TitleArg{ "title" 
    //                   , "creates a new project, <_>" }
    //     , create_project( kmap )
    //     }
    ,   { "create.recipe"
        , "creates a new recipe node"
        , ArgumentList{}
            | TitleArg{ "title" 
                      , "creates a new recipe, with title: <_>" }
        , create_recipe( kmap )
        }
    // ,   { "create.reference"
    //     , "creates a reference for selected node"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "reference node path"
    //                         , "creates a reference to <_>"
    //                         , kmap }
    //         | HeadingPathArg{ "target path"
    //                         , "creates a reference to <0> for <_>"
    //                         , kmap }
    //             | Attr::optional
    //     , create_reference( kmap )
    //     }
    ,   { "create.step" 
        , "creates a new recipe, as a step of the present recipe"
        , ArgumentList{}
            | RecipeArg{ "title" 
                       , "creates a new recipe, <_>, as a step of the present recipe"
                       , kmap }
        , create_step( kmap )
        }
    // ,   { "create.sibling"
    //     , "inserts node adjacent to selected node"
    //     , ArgumentList{}
    //         | TitleArg{ "sibling title"
    //                   , "inserts <_> adjacent to selected node" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
    //     , create_sibling( kmap )
    //     }
    ,   { "create.tag"
        , "creates a new tag definition"
        , ArgumentList{}
            | TagPathArg{ "heading"
                        , "creates tag <_>" // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
                        , kmap }
        , create_tag( kmap )
        }
    // ,   { "create.task" 
    //     , "creates a new project, as a task of the present project"
    //     , ArgumentList{}
    //         | TitleArg{ "title" 
    //                   , "creates a new project, <_>, as a task of the present project" }
    //     , create_task( kmap )
    //     }
    // ,   { "create.taint.project" 
    //     , "creates a new taint analysis project"
    //     , ArgumentList{}
    //         | TitleArg{ "title" 
    //                   , "creates a new taint anlaysis project" }
    //     , create_taint_project( kmap )
    //     }
    // ,   { "echo" 
    //     , "repeats argument to command line"
    //     , ArgumentList{}
    //         | TitleArg{ "title" 
    //                   , "repeats argument to command line" }
    //     , echo( kmap )
    //     }
    // ,   { "open.project" 
    //     , "places selected project in open status"
    //     , ArgumentList{}
    //         | ProjectArg{ "heading" 
    //                     , "changes the status of a project to open"
    //                     , kmap }
    //             | Attr::optional
    //     , open_project( kmap )
    //     }
    // ,   { "close.project" 
    //     , "places selected project in closed status"
    //     , ArgumentList{}
    //     , close_project( kmap )
    //     }
    // ,   { "copy.body"
    //     , "copies selected node body to a destination"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "heading"
    //                         , "copies selected node body to <_>"
    //                         , kmap }
    //     , copy_body( kmap )
    //     }
    ,   { "copy.state"
        , "copies present kmap state to a new location"
        , ArgumentList{}
            | FsPathArg{ "destination file path"
                       , "copies current state to <_>" } // TODO: replace "<_>" with user input.
        , copy_state( kmap )
        }
    // ,   { "count.ancestors"
    //     , "Returns the number of nodes in the selected path"
    //     , ArgumentList{}
    //     , count_ancestors( kmap )
    //     }
    // ,   { "count.descendants"
    //     , "Returns the number of descendant nodes in the selected path, inclusive"
    //     , ArgumentList{}
    //     , count_descendants( kmap )
    //     }
    ,   { "deactivate.project" 
        , "places selected project in open and deactivated status"
        , ArgumentList{}
            | ProjectArg{ "heading" 
                        , "deactivates <_>"
                        , kmap }
                | Attr::optional
        , deactivate_project( kmap )
        }
    // ,   { "delete.children" 
    //     , "removes selected node's children"
    //     , ArgumentList{}
    //     , delete_children( kmap )
    //     }
    // ,   { "delete.node" 
    //     , "removes selected non-alias node"
    //     , ArgumentList{}
    //     , delete_node( kmap )
    //     }
    ,   { "edit"
        , "opens node body for editing"
        , ArgumentList{}
        , edit_body( kmap )
        }
    ,   { "edit.title" 
        , "opens node title for editing"
        , ArgumentList{}
        , edit_title( kmap )
        }
    ,   { "execute"
        , "Runs script at current node"
        , ArgumentList{}
            | CommandPathArg{ "command path"
                            , "command path"
                            , kmap }
                | Attr::optional
        , execute_command( kmap )
        }
    ,   { "help"
        , "displays help contents"
        , ArgumentList{}
            | CommandArg{ "command"
                        , "displays the help content for [command](_)" // TODO: Use Markdown anchor/link syntax (as shown)?
                        , kmap }
                | Attr::optional
        , help( kmap )
        }
    ,   { "jump.in"
        , "jumps forward in the jump stack"
        , ArgumentList{}
            | JumpInArg{ "number in the jump stack"
                       , "jumps to <_>th elment in the jump stack"
                       , kmap }
                | Attr::optional
        , jump_in( kmap ) // TOOD: For starts, make the jump stack nonpersistent. Later feature will be to include it as part of DB state.
        }
    ,   { "jump.out"
        , "jumps backward in the jump stack"
        , ArgumentList{}
            | JumpOutArg{ "number in the jump stack"
                        , "jumps to <_>th elment in the jump stack"
                        , kmap }
                | Attr::optional
        , jump_out( kmap )
        }
    ,   { "list.commands"
        , "displays all available CLI commands"
        , ArgumentList{}
        , list_commands( kmap )
        }
    ,   { "load.call.map"
        , "loads call map"
        , ArgumentList{}
            | FsPathArg{ "path to itrace file"
                       , "loads itrace file at <_>" } 
            | FsPathArg{ "path to function map file"
                       , "loads function map file at <_>" } 
        , load_call_map( kmap )
        }
    ,   { "load.dot.file"
        , "Reads and parses a DOT file into a node hierarchy at the selected node"
        , ArgumentList{}
            | FsPathArg{ "DOT file path"
                       , "Render the <_> as a node hierarchy" }
        , load_dot( kmap )
        }
    ,   { "load.state"
        , "replaces present state with one from target database"
        , ArgumentList{}
            | FsPathArg{ "source file path"
                       , "replaces current state with <_>" } // TODO: replace "<_>" with user input.
        , load_state( kmap )
        }
    // ,   { "load.kscript"
    //     , "loads and executes a kmap script"
    //     , ArgumentList{}
    //         | FsPathArg{ "script file path"
    //                    , "loads and executes script <_>" } // TODO: replace "<_>" with user input.
    //     , load_script( kmap )
    //     }
    // ,   { "load.taint.data"
    //     , "initializes a taint analysis project"
    //     , ArgumentList{}
    //         | FsPathArg{ "ip_trace.bin file"
    //                    , "links ip trace file to project" }
    //         | FsPathArg{ "backlinks.bin file"
    //                    , "links back links file to project" }
    //         | FsPathArg{ "backlink_map.bin file"
    //                    , "links back link map file to project" }
    //         | FsPathArg{ "memory_origins.bin file"
    //                    , "contains addresses from whence memory originates" }
    //     , load_taint_data( kmap )
    //     }
    ,   { "merge.nodes"
        , "merges selected node to a destination"
        , ArgumentList{}
            | HeadingPathArg{ "heading"
                            , "merges selected node to <_>"
                            , kmap }
        , merge_nodes( kmap )
        }
    // ,   { "move"
    //     , "moves selected node to a destination"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "heading"
    //                         , "moves selected node to <_>"
    //                         , kmap }
    //     , move_node( kmap )
    //     }
    // ,   { "move.body"
    //     , "moves selected node body to a destination"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "heading"
    //                         , "moves selected node body to <_>"
    //                         , kmap }
    //     , move_body( kmap )
    //     }
    // ,   { "move.children"
    //     , "moves selected node's children to a destination"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "heading"
    //                         , "moves selected children node to <_>"
    //                         , kmap }
    //     , move_children( kmap )
    //     }
    // ,   { "move.down"
    //     , "moves selected node down one"
    //     , ArgumentList{}
    //     , move_down( kmap )
    //     }
    // ,   { "move.node"
    //     , "moves selected node to a destination"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "heading"
    //                         , "moves selected node to <_>"
    //                         , kmap }
    //     , move_node( kmap )
    //     }
    // ,   { "move.up"
    //     , "moves selected node above one"
    //     , ArgumentList{}
    //     , move_up( kmap )
    //     }
    ,   { "new"
        , "starts kmap state from scratch"
        , ArgumentList{}
            | FsPathArg{ "new database path"
                       , "creates new kmap state at <_>" }
                | Attr::optional
        , new_state( kmap )
        }
    // ,   { "propagate.taint.backward"
    //     , "performs backward taint propagation"
    //     , ArgumentList{}
    //         | UIntArg{ "Instruction index number"
    //                  , "back propagates from <_>" }
    //     , propagate_taint_backward( kmap )
    //     }
    // ,   { "repair.state"
    //     , "Analyzes Kmap database file for defects and attempts to fix them"
    //     , ArgumentList{}
    //         | FsPathArg{ "Kmap state file"
    //                    , "repairs state defects in file <_>" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
    //     , repair_state( kmap )
    //     }
    // ,   { "resolve.alias" 
    //     , "Selects the origin of an alias, if it exists"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "alias node path"
    //                         , "Selects the underlying origin node for alias given by <_>, if it exists" // TODO: replace "<_>" with user input.
    //                         , kmap }
    //             | Attr::optional
    //     , resolve_alias( kmap )
    //     }
    // ,   { "run.unit.tests"
    //     , "runs all unit tests"
    //     , ArgumentList{}
    //         // | HeadingArg{ "first argument forwarded to boost::unit_test::unit_test_main"
    //         //           , "forwards <_> to boost::unit_test::unit_test_main" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
    //         //      | Attr::optional
    //     , run_unit_tests( kmap )
    //     }
    ,   { "search.bodies"
        , "searches node bodies for regex comparison"
        , ArgumentList{}
            | TitleArg{ "regex string"
                      , "searches all node bodies for <_>" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
        , search_bodies( kmap )
        }
    ,   { "search.bodies.first"
        , "searches node bodies for regex comparison. Stops after first occurrence found"
        , ArgumentList{}
            | TitleArg{ "regex string"
                      , "searches all node bodies for one occurrence <_>" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
        , search_bodies_first( kmap )
        }
    ,   { "search.headings"
        , "searches node headings for regex comparison"
        , ArgumentList{}
            | TitleArg{ "regex string"
                      , "searches all node headings for <_>" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
        , search_headings( kmap )
        }
    ,   { "search.leaf.bodies"
        , "searches only leaf node bodies for regex comparison"
        , ArgumentList{}
            | TitleArg{ "regex string"
                      , "searches all leaf node bodies for <_>" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
        , search_leaf_bodies( kmap )
        }
    // ,   { "select.bookmark"
    //     , "selects bookmark"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "destination node path"
    //                         , "selects node given by <_>" // TODO: replace "<_>" with user input.
    //                         , bookmarks_root
    //                         , kmap }
    //     , select_bookmark( kmap )
    //     }
    // ,   { "select.daily.log"
    //     , "selects daily log node"
    //     , ArgumentList{}
    //     , select_daily_log( kmap )
    //     }
    ,   { "select.destination"
        , "selects alias destination"
        , ArgumentList{}
            | AliasDestArg{ "alias destination" // TODO: Need alias dest autocomplete.
                          , "selects alias destination <_>" // TODO: replace "<_>" with user input.
                          , kmap }
        , select_destination( kmap )
        }
    ,   { "select.next.fork"
        , "Selects node at next descendant fork, if found"
        , ArgumentList{}
        , next_fork( kmap )
        }
    ,   { "select.node"
        , "selects node"
        , ArgumentList{}
            | HeadingPathArg{ "destination node path"
                            , "selects node given by <_>" // TODO: replace "<_>" with user input.
                            , kmap }
        , select_node( kmap )
        }
    ,   { "select.prev.fork"
        , "Selects node at previous ancestor fork, if found"
        , ArgumentList{}
        , prev_fork( kmap )
        }
    ,   { "select.root"
        , "selects root node"
        , ArgumentList{}
        , select_root( kmap )
        }
    ,   { "select.source"
        , "selects alias source"
        , ArgumentList{}
        , select_source( kmap )
        }
    ,   { "store.resource"
        , "stores resource in database"
        , ArgumentList{}
            | FsPathArg{ "resource file path"
                       , "stores <_>" } // TODO: replace "<_>" with user input.
        , store_resource( kmap )
        }
    // ,   { "sort.children"
    //     , "Orders the children from top to bottom alphanumerically"
    //     , ArgumentList{}
    //     , sort_children( kmap )
    //     }
    // ,   { "swap"
    //     , "Swaps the location of selected node and provided path node"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "node path"
    //                         , "swap location of selected node and node given by <_>" // TODO: replace "<_>" with user input.
    //                         , kmap }
    //     , swap_node( kmap )
    //     }
    // ,   { "store.url.resource"
    //     , "stores url resource in database"
    //     , ArgumentList{}
    //         | UrlArg{ "url"
    //                 , "stores <_>" } // TODO: replace "<_>" with user input.
    //         // | HeadingPathArg{ "target path"
    //         //                 , "creates a reference to <0> for <_>"
    //         //                 , kmap }
    //         //     | Attr::optional
    //     , store_url_resource( kmap )
    //     }
#if 0
    ,   { "copy-node"
        , "copies node data from graph to clipboard"
        , copy_node( kmap )
        }
    ,   { "cut-node"
        , "copies node data from graph to clipboard, and removes it from graph"
        , cut_node( kmap )
        }
    ,   { "fold"
        , "hides descendant nodes"
        , ArgumentList{}
        , fold( kmap )
        }
    ,   { "paste-node"
        , "copies node data from clipboard to graph"
        , paste_node( kmap )
        }
    ,   { "view" // Opens the rendered text view. // Maybe replace with "open-view"? Or "open-preview"? That keeps the preview window opened or closed.
        , "opens node body for viewing, read-only, for present node or for <path> arg"
        , view_body( kmap )
        }
    ,   { "command-history"
        , "displays past 100 executed commands, chronologically"
        , history( kmap )
        }
    ,   { "navigation-history"
        , "displays past 100 nodes visited, chronologically"
        , history( kmap )
        }
    ,   { "load-plugin"
        , "Loads a kmap plugin"
        , load_plugin( kmap )
        }
    ,   { "rename"
        , "replaces node heading with arg"
        , rename( kmap )
        }
    ,   { "show-examples"
        , "displays available examples for the given command"
        , show_examples
        , CommandArg{}
        }
    ,   { "define"
        , "queries the definition nodes and displays the result"
        , define
        , HeadingPathArg
        }
    ,   { "schedule" // If we have tasks, it only seems reasonable that we have due dates / deadlines associated with those tasks.
        , "creates and attaches a reminder to a node to notify at a given time"
        , ArgumentList{}
            | DateAndTime{}
        , add_reminder( kmap )
        }
#endif // 0
    };
}

} // namespace kmap::cmd

