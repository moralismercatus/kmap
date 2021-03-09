/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "project.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../emcc_bindings.hpp"
#include "../error/master.hpp"
#include "../error/project.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "../lineage.hpp"
#include "command.hpp"

#include <boost/algorithm/string.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

#include <vector>
#include <string>

using namespace ranges;

namespace kmap::cmd {

auto is_in_project( Kmap const& kmap
                  , Uuid const& node )
    -> bool
{
    auto rv = false;

    if( auto const proot = fetch_project_root( kmap, node )
      ; proot )
    {
        rv = true;
    }

    return rv;
}

auto is_project( Kmap const& kmap
               , Lineal const& node )
    -> bool
{
    auto const target = node.leaf();

    return kmap.is_child( target
                        , "problem_statement" )
        && kmap.is_child( target
                        , "result" );
}

auto is_project( Kmap const& kmap
               , Uuid const& node )
    -> bool
{
    auto rv = false;

    if( auto const proot = kmap.fetch_leaf( "/projects" )
      ; proot )
    {
        if( auto const target = make< Lineal >( kmap, proot.value(), node )
          ; target )
        {
            rv = is_project( kmap
                           , target.value() );
        }
    }

    return rv;
}

auto is_category( Kmap const& kmap
                , Uuid const& node )
    -> bool
{
    return !is_project( kmap, node );
}

auto are_all_tasks_closed( Kmap const& kmap
                         , Uuid const& project )
    -> bool
{
    auto rv = false;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( is_project( kmap, project ) );
        })
    ;

    if( auto const cs = kmap.fetch_children( project, "tasks.open" )
      ; cs.empty() )
    {
        rv = true;
    }

    return rv;
}

auto get_result_node( Kmap const& kmap
                    , Uuid const& project )
    -> Uuid
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( is_project( kmap, project ) );
        })
    ;

    return kmap.fetch_child( project, "result" ).value();
}

auto fetch_projects_root( Kmap const& kmap )
    -> Result< Uuid >
{
    return kmap.fetch_descendant( "/projects" );
}

auto fetch_all_category( Kmap const& kmap )
    -> Result< Uuid >
{
    return kmap.fetch_descendant( "/projects.all" );
}

auto fetch_project_root( Kmap const& kmap
                       , Lineal const& node )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const [ root, target ] = node;

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( kmap.exists( rv.value() ) );
            }
        })
    ;

    if( is_project( kmap, node ) )
    {
        rv = target;
    }
    else
    {
        auto des = KMAP_TRY( make< Lineal >( kmap
                                           , root
                                           , kmap.fetch_parent( target ).value() ) );

        rv = fetch_project_root( kmap, des );
    }

    return rv;
}

auto fetch_project_root( Kmap const& kmap
                       , Uuid const& node )
    -> Result< Uuid >
{
    auto const pr = KMAP_TRY( fetch_projects_root( kmap ) );
    auto const des = KMAP_TRY( make< Lineal >( kmap, pr, node ) );

    return fetch_project_root( kmap, des );
}

auto fetch_project_status_root( Kmap const& kmap
                              , Uuid const& node )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const map_root = kmap.root_node_id();
    auto const inactive_root = KMAP_TRY( kmap.fetch_descendant( map_root, "/projects.open.inactive" ) );
    auto const active_root = KMAP_TRY( kmap.fetch_descendant( map_root, "/projects.open.active" ) );
    auto const closed_root = KMAP_TRY( kmap.fetch_descendant( map_root, "/projects.closed" ) );
    auto const roots = UuidVec{ inactive_root
                              , active_root
                              , closed_root };


    if( auto const it = find_if( roots
                               , [ & ]( auto const& e ){ return kmap.is_lineal( e, node ); } )
      ; it != roots.end() )
    {
        rv = *it;
    }

    return rv;
}

auto fetch_or_create_project_root( Kmap& kmap
                                 , Uuid const& node )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};
    auto const map_root = kmap.root_node_id();
    auto const inactive_root = kmap.fetch_or_create_descendant( map_root
                                                              , "projects.open.inactive" );
    auto const active_root = kmap.fetch_or_create_descendant( map_root
                                                            , "projects.open.active" );
    auto const closed_root = kmap.fetch_or_create_descendant( map_root
                                                            , "projects.closed" );
    auto const roots = UuidVec{ inactive_root.value()
                              , active_root.value()
                              , closed_root.value() };


    if( auto const it = find_if( roots
                               , [ & ]( auto const& e ){ return kmap.is_lineal( e, node ); } )
      ; it != roots.end() )
    {
        rv = *it;
    }

    return rv;
}

auto is_in_project_tree( Kmap& kmap
                       , Lineal const& node )
    -> bool
{
    return fetch_project_root( kmap
                             , node )
         . has_value();
}

auto is_project_category( Kmap& kmap
                        , Lineal const& node )
    -> bool
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    return !is_in_project_tree( kmap
                              , node );
}

auto fetch_nearest_project_category( Kmap& kmap
                                   , Lineal const& node )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const [ root, target ] = node;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( is_project_category( kmap, root, root ) ); // TODO: Re-enable when compiler support for lambda capture of structured bindings.
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( kmap.exists( rv.value() ) );
                // BC_ASSERT( root == rv || kmap.is_lineal( root, rv ) ); // TODO: Re-enable when compiler support for lambda capture of structured bindings.
                // BC_ASSERT( is_project_category( kmap, root, rv ) ); // TODO: Re-enable when compiler support for lambda capture of structured bindings.
            }
        })
    ;

    if( is_project_category( kmap, node ) )
    {
        rv = target;
    }
    else
    {
        auto const nn = KMAP_TRY( make< Lineal >( kmap
                                                , root
                                                , KMAP_TRY( kmap.fetch_parent( target ) ) ) );
        rv = fetch_nearest_project_category( kmap
                                           , nn );
    }

    return rv;
}

auto fetch_appropriate_project_category( Kmap& kmap
                                       , Uuid const& relative )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const all_root = KMAP_TRY( kmap.fetch_or_create_descendant( kmap.root_node_id()
                                                                   , "/projects.all" ) );
    if( auto const des = make< Lineal >( kmap, all_root, relative )
      ; des )
    {
        rv = fetch_nearest_project_category( kmap, des.value() );
    }
    else
    {
        rv = all_root;
    }

    return rv;
}

/**
 * TODO: There's a corner case here. Here's the scenario:
 *       1. Create project
 *       1. Move project to a new category (this is not reflected in open/closed categories)
 *       1. Project will not be deleted from previous open category, as the structure no longer matches.
 *       What may be required, then, is one of a few strategies:
 *       1. Create a "move" event that is fired whenever kmap.move() is called that updates aliases. This would cover all cases, I believe.
 *       1. Create a move command that activates on projects (guard=project), and updates aliases. The problem with this one is that direct calls
 *          to kmap.move() (via JS or C++) don't account for this. It seems that #1 is the only way to go.
 **/
auto close_project( Kmap& kmap
                  , Uuid const& project )
    -> Result< Uuid > 
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_project( kmap, rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( rv, is_project( kmap, project ), error_code::project::invalid_project );

    rv = update_project_status( kmap
                              , project
                              , ProjectCategory::closed );

    return rv;
}

auto open_project( Kmap& kmap
                 , Uuid const& project )
    -> Result< Uuid > 
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_project( kmap, rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( rv, is_project( kmap, project ), error_code::project::invalid_project );

    rv = update_project_status( kmap
                              , project
                              , ProjectCategory::inactive );

    return rv;
}

auto update_project_status( Kmap& kmap
                          , Uuid const& project
                          , ProjectCategory const& cat )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
        BC_POST([ & ]
        {
        })
    ;

    KMAP_ENSURE( rv, is_project( kmap, project ), error_code::project::invalid_project );

    auto const heading_map = std::map< ProjectCategory, std::string >
    {
        { ProjectCategory::active, "/open.active" }
    ,   { ProjectCategory::inactive, "/open.inactive" }
    ,   { ProjectCategory::closed, "/closed" }
    };
    auto const new_stat_heading = heading_map.at( cat );
    auto const projects_root = KMAP_TRY( fetch_projects_root( kmap ) );
    auto const all_cat = KMAP_TRY( kmap.fetch_descendant( projects_root, projects_root, "all" ) );
    auto const status_cat = KMAP_TRY( kmap.fetch_or_create_descendant( projects_root, new_stat_heading ) );
    auto const parent = KMAP_TRY( kmap.fetch_parent( project ) );
    auto const a_to_p = KMAP_TRY( make< Lineal >( kmap, all_cat, parent ) );
    auto const a_to_p_range = KMAP_TRY( make< LinealRange >( kmap, a_to_p ) );
    auto const src_cats = a_to_p_range | views::drop( 1 ) | to_lineal_range();
    auto const mirrored = KMAP_TRY( mirror_basic( kmap, src_cats, status_cat ) );

    rv = KMAP_TRY( kmap.create_alias( project, mirrored ) );

    auto const src_lin = [ & ]
    {
        auto t = LinealRange{ src_cats };
        t.push_back( project );
        return t;
    }();

    for( auto const& prefix : heading_map
                            | views::remove( cat, &decltype( heading_map )::value_type::first )
                            | views::values )
    {
        auto const heading = io::format( "{}.{}", prefix, to_heading_path( kmap, src_lin ) );

        if( auto const des = kmap.fetch_descendant( projects_root, projects_root, heading )
          ; des )
        {
            if( kmap.resolve( des.value() ) == project )
            {
                KMAP_TRY( kmap.delete_alias( des.value() ) );
            }
            else
            {
                rv = KMAP_MAKE_ERROR( error_code::project::invalid_project );

                break;
            }
        }

        // TODO: Need to update tasks.
    }

    return rv;
}

auto create_project( Kmap& kmap
                   , Uuid const& parent
                   , Heading const& heading )
    -> Result< Uuid > 
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_project( kmap, rv.value() ) );
            }
        })
    ;

    if( !kmap.is_child( parent, heading ) )
    {
        auto const proj = KMAP_TRY( kmap.create_child( parent, heading ) );
        
        KMAP_TRY( kmap.create_child( proj, "problem_statement" ) ); // TODO: change problem_statement to just "problem". Statement is redundant, or at least inconsistent with "result" ("result_statement"?)
        KMAP_TRY( kmap.create_child( proj, "result" ) );

        KMAP_TRY( update_project_status( kmap
                                       , proj
                                       , ProjectCategory::inactive ) );

        rv = proj;
    }

    return rv;
}

auto create_project( Kmap& kmap
                   , Title const& title )
    -> Result< Uuid > 
{
    auto const parent = KMAP_TRY( fetch_appropriate_project_category( kmap
                                                                    , kmap.selected_node() ) );
    auto const heading = format_heading( title );

    return create_project( kmap
                         , parent
                         , heading );
}

auto tasks_of( Kmap const& kmap
             , Uuid const& project )
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};
    auto const& db = kmap.database();

    if( auto const tasks = db.fetch_child( "tasks"
                                         , project )
      ; tasks )
    {
        if( auto const open = db.fetch_child( "open"
                                            , *tasks )
          ; open )
        {
            auto const cids = kmap.fetch_children( *open );

            rv.insert( rv.begin()
                     , cids.begin()
                     , cids.end() );
        }
        if( auto const closed = db.fetch_child( "closed"
                                              , *tasks )
          ; closed )
        {
            auto const cids = kmap.fetch_children( *closed );

            rv.insert( rv.begin()
                     , cids.begin()
                     , cids.end() );
        }
    }

    return rv;
}

auto fetch_project_status( Kmap const& kmap
                         , Heading const& heading )
    -> Optional< Uuid >
{
    auto const inactive_root = kmap.fetch_leaf( "/projects.open.inactive" );
    auto const active_root = kmap.fetch_leaf( "/projects.open.active" );
    auto const closed_root = kmap.fetch_leaf( "/projects.closed" );

    // TODO: I don't believe this should ever fail, except in circumstances
    // that would throw an exception; therefore, replace assert with throw.
    BC_ASSERT( inactive_root
            && active_root
            && closed_root );

    if( kmap.is_child( *inactive_root
                     , heading ) )
    {
        return *inactive_root;
    }
    else if( kmap.is_child( *active_root
                          , heading ) )
    {
        return *active_root;
    }
    else 
    {
        return *closed_root;
    }
}

auto fetch_project_status( Kmap const& kmap
                         , Uuid const& id )
    -> Optional< Uuid >
{
    auto const inactive_root = kmap.fetch_leaf( "/projects.open.inactive" );
    auto const active_root = kmap.fetch_leaf( "/projects.open.active" );
    auto const closed_root = kmap.fetch_leaf( "/projects.closed" );

    // TODO: I don't believe this should ever fail, except in circumstances
    // that would throw an exception; therefore, replace assert with throw.
    // Well.. could a user delete one of these? Surely better served with a Result<>.
    BC_ASSERT( inactive_root
            && active_root
            && closed_root );

    if( kmap.is_child( *inactive_root
                     , id ) )
    {
        return *inactive_root;
    }
    else if( kmap.is_child( *active_root
                          , id ) )
    {
        return *active_root;
    }
    else 
    {
        return *closed_root;
    }
}

auto fetch_project_status_alias( Kmap const& kmap
                               , Uuid const& project )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const open_root = kmap.fetch_descendant( "/projects.open" );
    auto const closed_root = kmap.fetch_descendant( "/projects.closed" );

    // auto const status_roots = { KMAP_TRY( open_root )}
    auto const aliases = [ & ]
    {
       auto const is_in_status = [ & ]( auto const& e )
       {
           return ( open_root && kmap.is_ancestor( open_root.value(), e ) )
               || ( closed_root && kmap.is_ancestor( closed_root.value(), e ) );
       };
       auto const all = kmap.fetch_aliases_to( project );

       return all
            | views::filter( is_in_status )
            // TODO: Needs to filter out tasks, as well. These need to be top-level projects.
            | to< decltype( all ) >();
    }();

    KMAP_ENSURE( rv, aliases.size() == 1, error_code::project::ambiguous_status );

    rv = aliases.back();

    return rv;
}

auto is_task( Kmap const& kmap
            , Uuid const& id )
    -> bool
{
    return kmap.fetch_ancestor( id, "closed.tasks" )
        || kmap.fetch_ancestor( id, "open.tasks" );
}

// TODO: It would seem that a category named "open" or "closed" would compromise this function e.g.: open.inactive.misc.closed.t1 would erroneously denote project "t1" as closed.
// I now realize this refers only to in_project_tree, but I think the concern still holds in this context.
auto fetch_task_status( Kmap const& kmap
                      , Uuid const& id )
    -> Optional< Uuid >
{
    auto const open = kmap.fetch_ancestor( id
                                         , "open" );
    auto const closed = kmap.fetch_ancestor( id
                                           , "closed" );

    BC_ASSERT( open || closed );

    if( open )
    {
        return open;
    }
    else
    {
        return closed;
    }
}

// Updates task aliases to project.
auto update_task_statuses( Kmap& kmap
                         , Uuid const& project
                         , Heading const& status )
    -> void
{
    auto const present_status = fetch_project_status( kmap
                                                    , project );

    BC_ASSERT( present_status );

    auto const full_status = [ & ]
    {
        if( status == "active"
         || status == "inactive" )
        {
            return "open." + status;
        }
        else
        {
            return status;
        }
    }();
    auto const top_status = [ & ]
    {
        if( status == "active"
         || status == "inactive" )
        {
            return Heading{ "open" };
        }
        else
        {
            return status;
        }
    }();

    if( kmap.fetch_heading( *present_status ).value() != status )
    {
        kmap.move_node( project, fmt::format( "/projects.{}", full_status ) );

        auto const aliases = kmap.fetch_aliases_to( project );
        auto const task_aliases = aliases
                                | views::remove_if( [ & ]( auto const& e ){ return !is_task( kmap, e ); } )
                                | to< std::vector< Uuid > >();

        for( auto const& task : task_aliases )
        {
            auto const task_status = fetch_task_status( kmap
                                                      , task );
            BC_ASSERT( task_status );

            if( kmap.fetch_heading( *task_status ).value() != status )
            {
                auto const tasks_root = kmap.fetch_ancestor( *task_status
                                                           , "tasks" );
                BC_ASSERT( tasks_root );
                auto const task_dst = kmap.fetch_or_create_leaf( *tasks_root
                                                               , fmt::format( ".{}.{}"
                                                                            , kmap.fetch_heading( *tasks_root ).value()
                                                                            , top_status ) );
                BC_ASSERT( task_dst );

                kmap.move_node( task, *task_dst );
            }
        }
    }
}

auto fetch_project( Kmap& kmap
                  , Heading const& heading )
    -> Optional< Uuid > 
{
    auto const status_root = fetch_project_status( kmap
                                                 , heading );
    auto const& db = kmap.database();

    return db.fetch_child( heading
                         , *status_root );
}

auto fetch_categorical_lineage( Kmap const& kmap
                              , Uuid const& root
                              , Uuid const& node )
    -> UuidVec
{
    auto rv = UuidVec{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.is_lineal( root, node ) );
        })
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( kmap.is_lineal( root, e ) );
                BC_ASSERT( !is_project( kmap, e ) );
            }
        })
    ;

    auto current = [ & ]
    {
        if( is_project( kmap, node ) )
        {
            return kmap.fetch_parent( node ).value();
        }
        else
        {
            return node;
        }
    }();

    while( current != root )
    {
        rv.emplace_back( current );

        current = kmap.fetch_parent( current ).value();
    }

    return rv | views::reverse | to< UuidVec >();
}

auto fetch_categorical_lineage( Kmap const& kmap
                              , Uuid const& node )
    -> UuidVec
{
    auto rv = UuidVec{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            auto const root = *kmap.fetch_leaf( "/projects" );
            BC_ASSERT( kmap.is_lineal( root, node ) );
        })
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                auto const root = *kmap.fetch_leaf( "/projects" );
                BC_ASSERT( kmap.is_lineal( root, e ) );
                BC_ASSERT( !is_project( kmap, e ) );
            }
        })
    ;

    auto const root = *kmap.fetch_leaf( "/projects" );

    rv = fetch_categorical_lineage( kmap
                                  , root
                                  , node );
    
    return rv;
}

auto fetch_user_categorical_lineage( Kmap const& kmap
                                   , Uuid const& node )
    -> UuidVec 
{
    auto rv = UuidVec{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            auto const root = *kmap.fetch_leaf( "/projects" );
            BC_ASSERT( kmap.is_lineal( root, node ) );
        })
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                auto const root = *kmap.fetch_leaf( "/projects" );
                BC_ASSERT( kmap.is_lineal( root, e ) );
                BC_ASSERT( !is_project( kmap, e ) );
            }
        })
    ;

    if( auto const cats = fetch_categorical_lineage( kmap, node )
      ; !cats.empty() )
    {
        using boost::algorithm::starts_with;

        auto const hp = to_heading_path_flat( kmap, cats );

        fmt::print( "user cats: {}\n", hp );

        if( starts_with( hp, "closed" ) )
        {
            rv = cats
               | views::drop( 1 )
               | to< UuidVec >();
        }
        else if( starts_with( hp, "open.active" )
              || starts_with( hp, "open.inactive" ) )
        {
            rv = cats
               | views::drop( 2 )
               | to< UuidVec >();
        }
    }

    return rv;
}

// TODO: What happens if a user aliases a category that can be laundered i.e., deleted?
// One solution is to have an "all" category, and have open.* and closed simply be references.
// This way, an alias to a category would resolve to the original, unchanging category.
// On second thought, I don't think this would solve the problem: The categories would still need to be real nodes,
// not aliases, to separate what is and is not open/closed/activated/deactivated.
auto launder_categories( Kmap& kmap
                       , Uuid const& leaf_category )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( leaf_category ) );
            BC_ASSERT( is_category( kmap, leaf_category ) );
        })
    ;

    if( auto const child_ps = kmap.fetch_children( leaf_category )
      ; child_ps.empty() )
    {
        if( auto const lin = fetch_user_categorical_lineage( kmap, leaf_category )
          ; !lin.empty() )
        {
            kmap.delete_node( lin.front() );
        }
    }
}

auto activate_project( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0
                        || args.size() == 1 );
            })
        ;

        auto const pid = [ & ]() -> Optional< Uuid >
        {
            if( args.size() == 0 )
            {
                if( auto const pp = fetch_project_root( kmap
                                                      , kmap.selected_node() )
                  ; pp )
                {
                    return { pp.value() };
                }
                else
                {
                    return {};
                }
            }
            else
            {
                auto const heading = args[ 0 ];

                return fetch_project( kmap
                                    , heading );
            }
        }();
        
        if( !pid )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "project not found" );
        }

        auto const status = fetch_project_status( kmap
                                                , *pid );
        auto const& db = kmap.database();

        if( *db.fetch_heading( *status ) != "active" ) // TODO: Probably unnecessary. Self-moves should be disregarded automatically.
        {
            update_task_statuses( kmap
                                , *pid
                                , "active" );
            KMAP_TRY( kmap.select_node( *pid ) );
        }

        return fmt::format( "project activated" );
    };
}

auto deactivate_project( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0
                        || args.size() == 1 );
            })
        ;

        auto const pid = [ & ]() -> Optional< Uuid >
        {
            if( args.size() == 0 )
            {
                if( auto const pp = fetch_project_root( kmap
                                                      , kmap.selected_node() )
                  ; pp )
                {
                    return { pp.value() };
                }
                else
                {
                    return {};
                }
            }
            else
            {
                auto const heading = args[ 0 ];

                return fetch_project( kmap
                                    , heading );
            }
        }();
        
        if( !pid )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "project not found" );
        }

        auto const status = fetch_project_status( kmap
                                                , *pid );
        auto const& db = kmap.database();

        if( *db.fetch_heading( *status ) != "inactive" ) // TODO: Probably unnecessary. Self-moves should be disregarded automatically.
        {
            // kmap.move_node( *pid
            //               , "/projects.open.inactive" );
            update_task_statuses( kmap
                                , *pid
                                , "inactive" );
            KMAP_TRY( kmap.select_node( *pid ) );
        }

        return fmt::format( "project deactivated" );
    };
}

namespace close_project_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
if( kmap.is_in_project( kmap.selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not in project tree' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();
const res = kmap.close_project( selected );

if( res.has_value() )
{
    rv = kmap.success( 'project closed' );
}
else
{
    rv = kmap.failure( res.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "closes nearest parent project";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_in_project"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    close.project
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace close_project_def

namespace dress_project_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
if( kmap.is_in_project( kmap.selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not in project tree' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();
const res = kmap.reorder_by_heading( [ 'problem_statement'
                                     , 'tasks'
                                     , 'result' ] );

if( res.has_value() )
{
    rv = kmap.success( 'project ordered to standard form' );
}
else
{
    rv = kmap.failure( res.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "reorders project nodes according to standard form";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_in_project"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    dress
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace dress_project_def

namespace open_project_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
if( kmap.is_in_project( kmap.selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not in project tree' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();
const res = kmap.open_project( selected );

if( res.has_value() )
{
    rv = kmap.success( 'project opened' );
}
else
{
    rv = kmap.failure( res.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "opens nearest parent project";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_in_project"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    open.project
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace open_project_def

namespace create_project_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();
const title = args.get( 0 );
const project = kmap.create_project( selected, title )

if( project.has_value() )
{
    const pstatus = kmap.fetch_project_status_alias( project.value() );

    kmap.select_node( pstatus.value() );

    rv = kmap.success( 'project created' );
}
else
{
    rv = kmap.failure( project.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates a project at nearest project category";
auto const arguments = std::vector< Argument >{ Argument{ "project_title"
                                                        , "title for project"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.project
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace create_project_def

namespace select_parent_project_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
const selected = kmap.selected_node();
if( kmap.is_project( selected )
 && kmap.is_single_parent_project( selected ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not a single parent project' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const selected = kmap.selected_node();
const parent_project = kmap.fetch_parent_project( selected ).value();

rv = kmap.success( 'parent project selected' );

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "selects the parent project of selected node";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unique_child_project" // Note: If a project is a subtask of more than one project, presumably, it would need to take an argument disambiguating which parent is to be selected.
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    select.parent.project
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace select_parent_project_def

namespace project::binding {

using namespace emscripten;

// TODO: This should actually not auto-deduce "appropriate" project, but leave it to the caller to do this, no? Keep consistent as an "interface wrapper".
auto close_project( Uuid const& at )
    -> kmap::binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();
    auto const project = KMAP_TRY( cmd::fetch_project_root( kmap, at ) );
    auto id = cmd::close_project( kmap
                                , project );

    return id;
}

// TODO: This should actually not auto-deduce "appropriate" parent, but leave it to the caller to do this, no? Keep consistent as an "interface wrapper".
auto create_project( Uuid const& at
                   , std::string const& title )
    -> kmap::binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();
    auto const heading = format_heading( title );
    auto const parent = KMAP_TRY( cmd::fetch_appropriate_project_category( kmap, at ) );
    auto id = cmd::create_project( kmap
                                 , parent
                                 , heading );

    if( id )
    {
        kmap.update_title( id.value(), title ); // TODO: This probably doesn't belong here. Either in the JS, or cmd::create_project. This should only be an interface wrapper.
    }

    return id;
}

auto fetch_project_root( Uuid const& node )
    -> kmap::binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return cmd::fetch_project_root( kmap, node );
}

auto fetch_project_status_root( Uuid const& node )
    -> kmap::binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return cmd::fetch_project_status_root( kmap, node );
}

auto fetch_project_status_alias( Uuid const& node )
    -> kmap::binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return cmd::fetch_project_status_alias( kmap, node );
}

auto is_in_project( Uuid const& node )
    -> bool 
{
    auto& kmap = Singleton::instance();

    return cmd::is_in_project( kmap, node );
}

auto is_project( Uuid const& node )
    -> bool 
{
    auto& kmap = Singleton::instance();

    return cmd::is_project( kmap, node );
}

// TODO: This should actually not auto-deduce "appropriate" project, but leave it to the caller to do this, no? Keep consistent as an "interface wrapper".
auto open_project( Uuid const& at )
    -> kmap::binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();
    auto const project = KMAP_TRY( cmd::fetch_project_root( kmap, at ) );
    auto id = cmd::open_project( kmap
                               , project );

    return id;
}

EMSCRIPTEN_BINDINGS( kmap_module )
{
    function( "close_project", &kmap::cmd::project::binding::close_project );
    function( "create_project", &kmap::cmd::project::binding::create_project );
    function( "fetch_project_root", &kmap::cmd::project::binding::fetch_project_root );
    function( "fetch_project_status_alias", &kmap::cmd::project::binding::fetch_project_status_alias );
    function( "is_in_project", &kmap::cmd::project::binding::is_in_project );
    function( "is_project", &kmap::cmd::project::binding::is_project );
    function( "open_project", &kmap::cmd::project::binding::open_project );
}

} // namespace taint::binding
} // namespace kmap::cmd