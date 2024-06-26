/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cmd.hpp"

#include "arg.hpp"
// #include "cmd/bookmark.hpp"
#include "cmd/cardinality.hpp"
#include "cmd/command.hpp"
#include "cmd/conclusion.hpp"
#include "cmd/definition.hpp"
#include "cmd/dotlang.hpp"
// #include "cmd/echo.hpp"
#include "cmd/next_fork.hpp"
#include "cmd/node_manip.hpp"
#include "cmd/project.hpp"
#include "cmd/recipe.hpp"
// #include "cmd/repair.hpp"
#include "cmd/resource.hpp"
// #include "cmd/script.hpp"
#include "cmd/search.hpp"
#include "cmd/select_node.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "test/master.hpp"
#include "com/text_area/text_area.hpp"
#include <path/node_view2.hpp>

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
auto fetch_leaf( com::CliCommand::Args const& args
               , Kmap const& kmap )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( args.size() >= Index + 1 )
    {
        auto const p = anchor::abs_root
                     | view2::desc( args[ Index ]  )
                     | act2::to_node_set();
        
        rv = !p.empty() ? *p.value().begin() : Optional< Uuid >{}; // TODO: This is clearly wrong. Simply selected the "first" possible path returned is not good practice!
    }

    return rv;
}

template< size_t Index >
auto fetch_leaf_or_selected( com::CliCommand::Args const& args
                           , Kmap const& kmap )
    -> Optional< Uuid >
{
    auto rv = fetch_leaf< Index >( args
                                 , kmap );
    if( !rv
     && args.size() < Index + 1 )
    {
        rv = nw->selected_node();
    }

    return rv;
}

auto list_commands( Kmap& kmap )
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        (void)args;
        
        auto filter_key = []( auto const& c )
        {
            return c.command;
        };
        auto const cli = KTRYE( kmap.fetch_component< com::Cli >() );
        auto const cmds = cli->valid_commands();
        auto const keys = cmds
                        | views::transform( filter_key )
                        | to_vector
                        | actions::sort;
        auto flat = keys
                  | views::join( '\n' )
                  | to< std::string >();
        auto const tv = KTRYE( kmap.fetch_component< com::TextArea >() );

        tv->clear();
        tv->show_preview( flat );

        return std::string{ "list-command" };
    };
}

auto help( Kmap& kmap )
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
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

            auto const cli = KTRYE( kmap.fetch_component< com::Cli >() );

            if( args.empty() )
            {
                auto const cmds = cli->valid_commands();
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
                auto const cmds = cli->valid_commands();
                auto const cmd_it = find_if( cmds
                                           , [ & ]( auto const& e ){ return e.command == args[ 0 ]; } );

                // TODO: Handle fail case (need to return failure with
                // unrecognized command? Or Should com::CliCommand->is_malformed
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

        auto const tv = KTRYE( kmap.fetch_component< com::TextArea >() );

        tv->clear();
        tv->hide_editor();
        tv->show_preview( markdown_to_html( out_text ) );

        return std::string{ "See preview" };
    };
}

auto jump_in( Kmap& kmap )
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.empty() || args.size() == 1 );
            })
        ;

        auto const nw = KTRYE( fetch_component< com::Network >() );
        auto& js = kmap.jump_stack(); 
        auto const rv = [ & ] // TODO: Try to reuse kmap::jump_out
        {
            // auto nid = js.jump_in();

            // BC_ASSERT( nid == nw->selected_node() );

            // (void)nid;

            return js.jump_in();
        }();

        if( !rv )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "jump stack empty" ) );
        }
        else
        {
            KMAP_TRY( kmap.jump_to( *rv ) );

            return fmt::format( "{} entered", KTRYE( nw->fetch_title( *rv ) ) );
        }
    };
}

auto jump_out( Kmap& kmap )
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
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

            // BC_ASSERT( nid == nw->selected_node() );

            // (void)nid;

            return js.jump_out();
        }();

        if( !rv )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "jump stack empty" ) );
        }
        else
        {
            KMAP_TRY( kmap.select_node( *rv ) );

            return fmt::format( "{} entered", kmap.fetch_title( *rv ).value() );
        }
    };
}

auto edit_title( Kmap& kmap )
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
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
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "target node not found" ) );
        }
        else
        {
            auto const cli = KTRYE( kmap.fetch_component< com::Cli >() );

            cli->clear_input();
            cli->show_popup( "enter title" );

            auto const text = cli->get_input();

            // TODO: this needs to ensure that any aliases dependent on this
            // get dynamically updated as well. The body isn't an issue, as the
            // preview will resolve the alias to the original/real node.
            KMAP_TRY( nw->update_title( *target, text ) );

            return std::string{ "node title open for editing" };
        }
    };
}

} // anonymous ns

auto make_core_commands( Kmap& kmap )
    -> std::vector< com::CliCommand >
{
    return std::vector< com::CliCommand >
    {
        // { "absolute.path"
        // , "displays full node path"
        // , ArgumentList{}
        //     | HeadingPathArg{ "node path"
        //                     , "displays path for <_>" 
        //                     , kmap }
        //         | Attr::optional
        // , absolute_path( kmap )
        // }
    ,   { "activate.project" 
        , "places selected project in open and activated status"
        , ArgumentList{}
            | ProjectArg{ "heading" 
                        , "activates <_>"
                        , kmap }
                | Attr::optional
        , activate_project( kmap )
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
    // ,   { "create.bookmark"
    //     , "bookmarks selected node"
    //     , ArgumentList{}
    //     , create_bookmark( kmap )
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
    // ,   { "echo" 
    //     , "repeats argument to command line"
    //     , ArgumentList{}
    //         | TitleArg{ "title" 
    //                   , "repeats argument to command line" }
    //     , echo( kmap )
    //     }
    // ,   { "copy.body"
    //     , "copies selected node body to a destination"
    //     , ArgumentList{}
    //         | HeadingPathArg{ "heading"
    //                         , "copies selected node body to <_>"
    //                         , kmap }
    //     , copy_body( kmap )
    //     }
    // ,   { "copy.state"
    //     , "copies present kmap state to a new location"
    //     , ArgumentList{}
    //         | FsPathArg{ "destination file path"
    //                    , "copies current state to <_>" } // TODO: replace "<_>" with user input.
    //     , copy_state( kmap )
    //     }
    // ,   { "count.ancestors"
    //     , "Returns the number of nodes in the selected path"
    //     , ArgumentList{}
    //     , count_ancestors( kmap )
    //     }
    // ,   { "delete.children" 
    //     , "removes selected node's children"
    //     , ArgumentList{}
    //     , delete_children( kmap )
    //     }
    // ,   { "edit.title" 
    //     , "opens node title for editing"
    //     , ArgumentList{}
    //     , edit_title( kmap )
    //     }
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
    ,   { "load.dot.file"
        , "Reads and parses a DOT file into a node hierarchy at the selected node"
        , ArgumentList{}
            | FsPathArg{ "DOT file path"
                       , "Render the <_> as a node hierarchy" }
        , load_dot( kmap )
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
    // ,   { "new"
    //     , "starts kmap state from scratch"
    //     , ArgumentList{}
    //         | FsPathArg{ "new database path"
    //                    , "creates new kmap state at <_>" }
    //             | Attr::optional
    //     , new_state( kmap )
    //     }
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
    // ,   { "search.leaf.bodies"
    //     , "searches only leaf node bodies for regex comparison"
    //     , ArgumentList{}
    //         | TitleArg{ "regex string"
    //                   , "searches all leaf node bodies for <_>" } // TODO: replace "<_>" with user input. Statically replace "<_>" with "this argument", dynamically what is typed. 
    //     , search_leaf_bodies( kmap )
    //     }
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

