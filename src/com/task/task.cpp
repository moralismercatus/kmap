/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/task/task.hpp>

#include "com/cmd/cclerk.hpp"
#include "com/network/network.hpp"
#include "com/tag/tag.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "error/result.hpp"
#include "kmap.hpp"
#include "path/act/update_body.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"
#include "util/result.hpp"
#include "utility.hpp"
#include <path/node_view2.hpp>
#include <path/act/order.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/stable_sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::com {

auto fetch_task_root( Kmap const& kmap )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    return KTRY( view::abs_root
               | view::child( "task" )
               | view::fetch_node( kmap ) );
}

auto fetch_task_root( Kmap& kmap )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    return KTRY( view::abs_root
               | view::child( "task" )
               | view::fetch_or_create_node( kmap ) );
}

TaskStore::TaskStore( Kmap& kmap
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ kmap, requisites, description }
    , eclerk_{ kmap }
    , cclerk_{ kmap }
{
    register_standard_commands();
}

auto TaskStore::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto TaskStore::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}

auto TaskStore::cascade_tags( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "task", task );

    auto rv = result::make_result< void >();
    auto& km = kmap_inst();
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const rtags = view::make( task )
                     | view::tag( view::none_of( "task.status.open.active", "task.status.open.inactive", "task.status.closed" ) )
                     | view::resolve
                     | view::to_node_set( km );
    auto const subtasks = view::make( task )
                        | view::desc( view::PredFn{ [ this ]( auto const& n ){ return is_task( n ); } } ) // TODO: Replace with `view::desc | view::task`
                        | view::to_node_set( km );

    for( auto const& subtask : subtasks )
    {
        for( auto const& rtag : rtags )
        {
            if( auto const exists = view::make( subtask )
                                  | view::tag( rtag )
                                  | view::exists( km )
              ; !exists )
            {
                KTRY( tag_store->tag_node( subtask, rtag ) );
            }
        }
    }

    rv = outcome::success();

    return rv;
}

auto TaskStore::create_task( std::string const& title )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "title", title );

    auto rv = result::make_result< Uuid >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_task( rv.value() ) );
            }
        })
    ;

    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const task_root = KTRY( fetch_task_root( km ) );
    auto const task = KTRY( nw->create_child( task_root, format_heading( title ), title ) );
    
    KTRY( view::make( task )
        | view::child( view::all_of( "problem", "result" ) )
        | view::create_node( km ) );

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.task" }, { { "task_id", to_string( task ) } } ) );

    KTRY( deactivate_task( task ) );

    rv = task;

    return rv;
}

SCENARIO( "create_task", "[task][create][tag][order]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& km = Singleton::instance();
    auto const tstore = REQUIRE_TRY( km.fetch_component< com::TaskStore >() );
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );

    GIVEN( "no tasks" )
    {
        GIVEN( "create.task 1" )
        {
            auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

            THEN( "structure correct" )
            {
                REQUIRE( 2 == ( view::make( t1 ) 
                              | view::child( view::all_of( "problem", "result" ) )
                              | view::count( km ) ) );
            }
            THEN( "1 is open and inactive" )
            {
                REQUIRE(( view::make( t1 )
                        | view::tag( "task.status.open.inactive" )
                        | view::exists( km ) ));
            }

            GIVEN( "create.task 2" )
            {
                auto const t2 = REQUIRE_TRY( tstore->create_task( "2" ) );

                THEN( "2 below 1" )
                {
                    auto const t1_pos = REQUIRE_TRY( nw->fetch_ordering_position( t1 ) );
                    auto const t2_pos = REQUIRE_TRY( nw->fetch_ordering_position( t2 ) );

                    REQUIRE( t1_pos < t2_pos );
                }
            }
        }
    }
}

auto TaskStore::create_subtask( Uuid const& supertask
                              , std::string const& title )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "supertask", supertask );
        KM_RESULT_PUSH_STR( "title", title );

    auto rv = result::make_result< Uuid >();

    KMAP_ENSURE( is_task( supertask ), error_code::common::uncategorized );

    auto& km = kmap_inst();
    auto const subtask_node = KTRY( create_task( title ) );

    KTRY( view::make( supertask )
        | view::child( "subtask" )
        | view::alias( view::Alias::Src{ subtask_node } )
        | view::create_node( km )
        | view::to_single );

    auto const st_alias = KTRY( anchor::node( supertask )
                              | view2::child( "subtask" )
                              | view2::alias( view2::resolve( subtask_node ) )
                              | act2::fetch_node( km ) );

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.created", "object.subtask" } ) );

    rv = st_alias;

    return rv;
}

// task = task_root | child( all_of( child( "problem" ), child( "result" ) ) )
// subtask = child( "subtask" ) | alias | current( task_root | task )
// What about stacking predicates... sure? Why not?:
// subtask = child( "subtask" )( parent( task ) ) | alias( task_root | task ) // OK: alias ready to accept another predicate i.e., name of subtask, etc.


SCENARIO( "TaskStore::create_subtask", "[task][create]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& km = Singleton::instance();
    auto tstore = REQUIRE_TRY( km.fetch_component< TaskStore >() );

    GIVEN( "no tasks" )
    {
        WHEN( "create subtask with task root as supertask" )
        {
            auto const troot = REQUIRE_TRY( fetch_task_root( km ) );

            REQUIRE( kmap::test::fail( tstore->create_subtask( troot, "1" ) ) );
        }
    }
    GIVEN( "create.task super" )
    {
        auto const super = REQUIRE_TRY( tstore->create_task( "super" ) );

        GIVEN( "create.subtask sub" )
        {
            auto const super_subtask_sub = REQUIRE_TRY( tstore->create_subtask( super, "sub" ) );

            THEN( "'sub' is task" )
            {
                REQUIRE( tstore->is_task( super_subtask_sub ) );
            }
            THEN( "result 'sub' is subtask to 'super'" )
            {
                auto const subalias = REQUIRE_TRY(( anchor::node( super )
                                                  | view2::child( "subtask" )
                                                  | view2::alias
                                                  | act2::fetch_node( km ) ));

                REQUIRE( subalias == super_subtask_sub );
            }

            GIVEN( "@super.subtask.sub :create.subtask subsub" )
            {
                auto const sub_subtask_subsub = REQUIRE_TRY( tstore->create_subtask( super_subtask_sub, "subsub" ) );

                THEN( "result 'subsub' is sub.subtask.subsub alias" )
                {
                    KTRYE( print_tree( km, super ) );
                    REQUIRE(( anchor::node( super )
                            | view2::child( "subtask" )
                            | view2::alias( "sub" )
                            | view2::child( "subtask" )
                            | view2::alias( sub_subtask_subsub )
                            | act2::exists( km ) ));
                }
                THEN( "'subsub' is task" )
                {
                    REQUIRE( tstore->is_task( sub_subtask_subsub ) );
                }
                THEN( "'subsub' is a subtask of resolve( 'sub' )" )
                {
                    // auto const rsub = REQUIRE_TRY(( anchor::node( sub_subtask_sub ) 
                    //                               | view2::resolve
                    //                               | act2::fetch_node( km ) ));
                    // auto const sub_subtask_sub = REQUIRE_TRY( anchor::node( rsub )
                    //                                         |  );
                }
                THEN( "'subsub' is subtask to 'sub'" )
                {
                    // auto subsubalias = REQUIRE_TRY( ( view::make( subalias )
                    //                                 | view::child( "subtask" )
                    //                                 | view::child
                    //                                 | view::fetch_node( km ) ) );

                    // REQUIRE( subsubalias == subsub );
                }
            }
        }
    }
}

// Return true if a tag other than `status.open` is found.
// TODO: Rather, task has tag other than tag.status desc found.
// TODO: What about task.status.closed?
auto TaskStore::is_categorized( Uuid const& task ) const
    -> bool
{
    auto& kmap = kmap_inst();
    auto const nonopen_tags = view::make( task )
                            | view::tag( view::none_of( "task.status.open.active", "task.status.open.inactive" ) )
                            | view::to_node_set( kmap );

    return !nonopen_tags.empty();
}

auto TaskStore::is_task( Uuid const& node ) const
    -> bool
{
    auto rv = false;
    auto& km = kmap_inst();

    if( auto const troot = fetch_task_root( km )
      ; troot )
    {
        auto const nw = KTRYE( km.fetch_component< com::Network >() );

        rv = view::make( troot.value() )
           | view::child( nw->resolve( node ) )
           | view::exists( km );
    }

    return rv;
}

auto TaskStore::close_task( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "task", task );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const task_root = KTRY( fetch_task_root( km ) );

    KMAP_ENSURE( is_task( task ), error_code::common::uncategorized );
    KMAP_ENSURE( !( view::make( task ) | view::tag( "task.status.closed" ) | view::exists( km ) ), error_code::common::uncategorized );
    KMAP_ENSURE( is_categorized( task ), error_code::common::uncategorized );

    // disallow closure of empty body results.
    auto const result_documented = [ & ]
    {
        auto const resultn = KTRYE( view::make( task )
                                  | view::child( "result" )
                                  | view::fetch_node( km ) );

        if( auto const body = nw->fetch_body( resultn )
          ; body )
        {
            return !body.value().empty();
        }
        else
        {
            return false;
        }
    }();
    
    if( result_documented && is_categorized( task ) )
    {
        // Remove open tag if it exists...
        {
            if( auto const v = view::make( task )
                         | view::tag( view::any_of( "task.status.open.active", "task.status.open.inactive" ) )
              ; v | view::exists( km ) )
            {
                KTRY( v | view::erase_node( km ) );
            }
        }
        // Append close tag...
        {
            KTRY( tag_store->fetch_or_create_tag( "task.status.closed" ) );

            KTRY( view::make( task )
                | view::tag( "task.status.closed" )
                | view::create_node( km ) );
        }

        // Order.
        {
            auto const ordered = KTRY( order_tasks( km ) );

            KTRY( nw->reorder_children( task_root, ordered ) );
        }

        KTRY( eclerk_.fire_event( { "subject.task_store", "verb.closed", "object.task" }, { { "task_id", to_string( task ) } } ) );
        // TODO: Provide outlet that moves closed node to top of all open task and subtask lists.

        rv = outcome::success();
    }

    return rv;
}

SCENARIO( "close_task", "[task][close]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& km = Singleton::instance();
    auto const tstore = REQUIRE_TRY( km.fetch_component< com::TaskStore >() );

    GIVEN( "no tasks" )
    {
        GIVEN( "close.task 1" )
        {
            auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

            REQUIRE(( view::make( t1 ) | view::tag( "task.status.open.inactive" ) | view::exists( km ) ));

            THEN( "close.task 1 fails" )
            {
                REQUIRE( test::fail( tstore->close_task( t1 ) ) );
            }
            GIVEN( "given category tag" )
            {
                auto const tag_store = REQUIRE_TRY( km.fetch_component< com::TagStore >() );

                REQUIRE_TRY( tag_store->create_tag( "test" ) );

                REQUIRE(( view::make( t1 ) | view::tag( "test" ) | view::create_node( km ) ));

                GIVEN( "result body nonempty" )
                {
                    REQUIRE_TRY(( view::make( t1 )
                                | view::child( "result" )
                                | act::update_body( km, "test" ) ));

                    WHEN( "close.task 1" )
                    {
                        REQUIRE_TRY( tstore->close_task( t1 ) );

                        THEN( "tag active exchanged for closed" )
                        {
                            REQUIRE( !( view::make( t1 ) | view::tag( "task.status.open.active" ) | view::exists( km ) ) );
                            REQUIRE(  ( view::make( t1 ) | view::tag( "task.status.closed" ) | view::exists( km ) ) );
                        }
                    }
                }
            }
        }
    }
}

auto TaskStore::activate_task( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "task", task );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& kmap = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const tag_store = KTRY( fetch_component< TagStore >() );

    KTRY( tag_store->fetch_or_create_tag( "task.status.open.active" ) );

    // Remove closed tag if it exists.
    if( auto const vt_closed = view::make( task )
                             | view::tag( "task.status.closed" )
      ; vt_closed | view::exists( kmap ) )
    {
        KTRY( vt_closed | view::erase_node( kmap ) );
    }
    if( auto const vt_inactive = view::make( task )
                               | view::tag( "task.status.open.inactive" )
      ; vt_inactive | view::exists( kmap ) )
    {
        KTRY( vt_inactive | view::erase_node( kmap ) );
    }
    // Append open tag.
    {
        KTRY( view::make( task )
            | view::tag( "task.status.open.active" )
            | view::create_node( kmap ) );
    }

    KTRY( nw->set_ordering_position( task, 0 ) );

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.open.activated", "object.task" }, { { "task_id", to_string( task ) } } ) );

    rv = outcome::success();

    return rv;
}

SCENARIO( "TaskStore::activate_task", "[task]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store" );

    auto& km = Singleton::instance();
    auto const tstore = REQUIRE_TRY( km.fetch_component< com::TaskStore >() );
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const push_closure_reqs = [ & ]( auto const& task )
    {
        auto const tag_store = REQUIRE_TRY( km.fetch_component< com::TagStore >() );
        /*auto const test_tag = */REQUIRE_TRY( tag_store->create_tag( "test" ) );

        REQUIRE_TRY(( view::make( task ) | view::tag( "test" ) | view::create_node( km ) ));
        REQUIRE_TRY(( anchor::node( task ) | view2::child( "result" ) | act2::update_body( km, "test" ) ));
    };

    GIVEN( "active task" )
    {
        auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

        WHEN( "activate.task" )
        {
            REQUIRE_TRY( tstore->activate_task( t1 ) );

            THEN( "active tag still applied" )
            {
                REQUIRE(( view::make( t1 )
                        | view::tag( "task.status.open.active" )
                        | view::exists( km ) ));
            }
        }
    }
    GIVEN( "inactive task" )
    {
        auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

        WHEN( "activate.task" )
        {
            REQUIRE_TRY( tstore->activate_task( t1 ) );

            THEN( "active tag applied" )
            {
                REQUIRE(( view::make( t1 )
                        | view::tag( "task.status.open.active" )
                        | view::exists( km ) ));
            }
        }
    }
    GIVEN( "closed task" )
    {
        auto const t1 = REQUIRE_TRY( tstore->create_task( "1" ) );

        push_closure_reqs( t1 );

        REQUIRE_TRY( tstore->close_task( t1 ) );

        WHEN( "activate.task" )
        {
            REQUIRE_TRY( tstore->activate_task( t1 ) );

            THEN( "active tag applied" )
            {
                REQUIRE(( view::make( t1 )
                        | view::tag( "task.status.open.active" )
                        | view::exists( km ) ));
            }
        }
    }
}

auto TaskStore::deactivate_task( Uuid const& task )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "task", task );

    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const tag_store = KTRY( fetch_component< TagStore >() );
    auto const nw = KTRY( fetch_component< Network >() );

    KTRY( tag_store->fetch_or_create_tag( "task.status.open.inactive" ) );

    // Remove closed tag if it exists.
    if( auto const vt_closed = view::make( task )
                             | view::tag( "task.status.closed" )
      ; vt_closed | view::exists( km ) )
    {
        KTRY( vt_closed | view::erase_node( km ) );
    }
    if( auto const vt_active = view::make( task )
                             | view::tag( "task.status.open.active" )
      ; vt_active | view::exists( km ) )
    {
        KTRY( vt_active | view::erase_node( km ) );
    }
    // Append open tag.
    {
        auto const vt_inactive = view::make( task )
                               | view::tag( "task.status.open.inactive" );
        KTRY( vt_inactive | view::create_node( km ) );
    }
    // Order.
    {
        auto const task_root = KTRYE( fetch_task_root( km ) );
        auto const ordered = KTRY( order_tasks( km ) );

        KTRY( nw->reorder_children( task_root, ordered ) );
    }

    KTRY( eclerk_.fire_event( { "subject.task_store", "verb.open.deactivated", "object.task" }, { { "task_id", to_string( task ) } } ) );

    rv = outcome::success();

    return rv;
}

auto TaskStore::register_standard_commands()
    -> void
{
    using Guard = com::Command::Guard;
    using Argument = com::Command::Argument;

    // arg.task_path
    {
        auto const guard_code =
        R"%%%(
            return kmap.is_valid_heading_path( arg );
        )%%%";
        auto const completion_code =
        R"%%%(
            const troot = kmap.fetch_node( "/task" );

            if( troot.has_error() )
            {
                return new kmap.VectorString();
            }
            else
            {
                return kmap.complete_heading_path_from( troot.value(), troot.value(), arg );
            }
        )%%%";

        auto const description = "task heading";
        
        cclerk_.register_argument( com::Argument{ .path = "task_heading"
                                                , .description = description
                                                , .guard = guard_code
                                                , .completion = completion_code } );
    }
    // add.subtask
    {
        auto const guard_code = 
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
        // TODO: Start here.
        // const supertask = kmap.selected_node();
        // const target = kmap.fetch_node( args.get( 0 ) );
        // Ensure target is task.
        // subtask = create_child( supertask, "subtask" );
        // create_alias( target, subtask );

        if( taskn.has_value() )
        {
            kmap.select_node( taskn.value() );

            return kmap.success( 'success' );
        }
        else
        {
            return kmap.failure( taskn.error_message() );
        }
        )%%%";
        auto const description = "adds existent task as subtask of current task";
        auto const arguments = std::vector< Argument >{ Argument{ "task_heading"
                                                                , "task heading"
                                                                , "task_heading" } };

        cclerk_.register_command( com::Command{ .path = "add.subtask"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "unconditional", guard_code }
                                              , .action = action_code } );
    }
    // activate.task
    {
        auto const guard_code = 
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
            console.log( '1' );
        const selected = kmap.selected_node();
            console.log( '2' );
        const res = kmap.task_store().activate_task( selected );
            console.log( '3' );

        if( res.has_value() )
        {
            console.log( '4' );
            kmap.select_node( selected );
            console.log( '5' );

            return kmap.success( 'success' );
        }
        else
        {
            console.log( '6' );
            return kmap.failure( res.error_message() );
        }
        )%%%";
        auto const description = "opens task and marks as inactive";
        auto const arguments = std::vector< Argument >{};

        cclerk_.register_command( com::Command{ .path = "activate.task"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", guard_code }
                                              , .action = action_code } );
    }
    // deactivate.task
    {
        auto const guard_code = 
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
        const selected = kmap.selected_node();
        const res = kmap.task_store().deactivate_task( selected );

        if( res.has_value() )
        {
            kmap.select_node( selected );

            return kmap.success( 'success' );
        }
        else
        {
            return kmap.failure( res.error_message() );
        }
        )%%%";
        auto const description = "opens task and marks as inactive";
        auto const arguments = std::vector< Argument >{};

        cclerk_.register_command( com::Command{ .path = "deactivate.task"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", guard_code }
                                              , .action = action_code } );
    }
    // close.task
    {
        auto const guard_code = // TODO: Guard should include require: categorized && open
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
        const selected = kmap.selected_node();
        const res = kmap.task_store().close_task( selected );

        if( res.has_value() )
        {
            kmap.select_node( selected );

            return kmap.success( 'success' );
        }
        else
        {
            return kmap.failure( res.error_message() );
        }
        )%%%";
        auto const description = "closes task meeting requirements";
        auto const arguments = std::vector< Argument >{};

        cclerk_.register_command( com::Command{ .path = "close.task"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", guard_code }
                                              , .action = action_code } );
    }
    // create.task
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'unconditional' );
        )%%%";
        auto const action_code =
        R"%%%(
        const tstore = kmap.task_store();
        const taskn = tstore.create_task( args.get( 0 ) );

        if( taskn.has_value() )
        {
            kmap.select_node( taskn.value() ).throw_on_error();

            return kmap.success( 'success' );
        }
        else
        {
            return kmap.failure( taskn.error_message() );
        }
        )%%%";
        auto const description = "creates task";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        cclerk_.register_command( com::Command{ .path = "create.task"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "unconditional", guard_code }
                                              , .action = action_code } );
    }
    // create.task.inactive
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'unconditional' );
        )%%%";
        auto const action_code =
        R"%%%(
        const tstore = kmap.task_store();
        const taskn = tstore.create_task( args.get( 0 ) );

        if( taskn.has_value() )
        {
            kmap.select_node( taskn.value() ).throw_on_error();

            return kmap.success( 'success' );
        }
        else
        {
            return kmap.failure( taskn.error_message() );
        }
        )%%%";
        auto const description = "creates task in inactive state";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        cclerk_.register_command( com::Command{ .path = "create.task.inactive"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "unconditional", guard_code }
                                              , .action = action_code } );
    }
    // create.subtask
    {
        auto const is_task_guard_code =
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
            const tstore = kmap.task_store();
            const taskn = tstore.create_subtask( kmap.selected_node(), args.get( 0 ) );

            if( taskn.has_value() )
            {
                kmap.select_node( taskn.value() ).throw_on_error();

                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( taskn.error_message() );
            }
        )%%%";
        auto const description = "creates subtask";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        cclerk_.register_command( com::Command{ .path = "create.subtask"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", is_task_guard_code }
                                              , .action = action_code } );
    }
    // create.subtask.inactive
    {
        auto const is_task_guard_code =
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
            const tstore = kmap.task_store();
            const taskn = tstore.create_subtask( kmap.selected_node(), args.get( 0 ) );

            if( taskn.has_value() )
            {
                kmap.select_node( taskn.value() ).throw_on_error();

                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( taskn.error_message() );
            }
        )%%%";
        auto const description = "creates subtask in inactive state";
        auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                                , "task title"
                                                                , "unconditional" } };

        cclerk_.register_command( com::Command{ .path = "create.subtask.inactive"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", is_task_guard_code }
                                              , .action = action_code } );
    }
    // cascade.tags
    {
        auto const is_task_guard_code =
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
            const cascaded = kmap.task_store().cascade_tags( kmap.selected_node() );

            if( cascaded.has_value() )
            {
                kmap.select_node( kmap.selected_node() );

                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( cascaded.error_message() );
            }
        )%%%";
        auto const description = "Applies supertask tags to subtasks";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "is_task", is_task_guard_code };

        cclerk_.register_command( com::Command{ .path = "cascade.tags"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", is_task_guard_code }
                                              , .action = action_code } );
    }
    // open.task
    {
        auto const guard_code = 
        R"%%%(
            if( kmap.task_store().is_task( kmap.selected_node() ) )
            {
                return kmap.success( 'success' );
            }
            else
            {
                return kmap.failure( 'not task' );
            }
        )%%%";
        auto const action_code =
        R"%%%(
        const selected = kmap.selected_node();
        const res = kmap.task_store().activate_task( selected );

        if( res.has_value() )
        {
            kmap.select_node( selected );

            return kmap.success( 'success' );
        }
        else
        {
            return kmap.failure( res.error_message() );
        }
        )%%%";
        auto const description = "opens closed task and marks it as active";
        auto const arguments = std::vector< Argument >{};

        cclerk_.register_command( com::Command{ .path = "open.task"
                                              , .description = description
                                              , .arguments = arguments 
                                              , .guard = Guard{ "is_task", guard_code }
                                              , .action = action_code } );
    }
}

auto is_order_regular( Kmap const& km )
    -> bool
{
    auto const children = view2::task::task_root
                        | view2::task::task
                        | act2::to_node_set( km )
                        | act::order( km );
    auto const ots = KTRYE( order_tasks( km ) );

    return children == ots;
}

auto order_tasks( Kmap const& km )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();

    enum class Status : int { active, inactive, closed, nontask }; // Ordered.

    auto rv = result::make_result< UuidVec >();
    auto const task_root = KTRYE( fetch_task_root( km ) );
    auto tasks = view::make( task_root )
               | view::child
               | view::to_node_set( km )
               | act::order( km );
    auto const map_status = [ & ]( auto const& n )
    {
             if( view::make( n ) | view::tag( "task.status.open.active" ) | view::exists( km ) )   { return Status::active; }
        else if( view::make( n ) | view::tag( "task.status.open.inactive" ) | view::exists( km ) ) { return Status::inactive; }
        else if( view::make( n ) | view::tag( "task.status.closed" ) | view::exists( km ) )        { return Status::closed; }
        else                                                                                       { return Status::nontask; }
    };
    auto status_map = tasks
                    | rvs::transform( [ & ]( auto const& t ){ return std::pair{ t, map_status( t ) }; } )
                    | ranges::to< std::map< Uuid, Status > >();
    auto const pred = [ & ]( auto const& t1, auto const& t2 )
    {

        return static_cast< int >( status_map.at( t1 ) ) < static_cast< int >( status_map.at( t2 ) ); 
    };

    ranges::stable_sort( tasks, pred ); // stable_sort maintains pre-sort order for equivalent items.

    rv = tasks;

    return rv;
}

} // namespace kmap

namespace {
namespace task_store_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::TaskStore
,   std::set({ "command_store"s, "event_store"s, "tag_store"s })
,   "task related functionality"
);

} // namespace task_store_def 

struct TaskStore
{
    kmap::Kmap& km;

    TaskStore( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto cascade_tags( kmap::Uuid const& task )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->cascade_tags( task );
    }
    auto create_task( std::string const& title )
        -> kmap::binding::Result< kmap::Uuid >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->create_task( title );
    }
    auto create_subtask( kmap::Uuid const& supertask
                       , std::string const& title )
        -> kmap::binding::Result< kmap::Uuid >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->create_subtask( supertask, title );
    }
    auto close_task( kmap::Uuid const& task )
                   
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->close_task( task );
    }
    auto activate_task( kmap::Uuid const& task )
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->activate_task( task );
    }
    auto deactivate_task( kmap::Uuid const& task )
                   
        -> kmap::binding::Result< void >
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->deactivate_task( task );
    }
    auto is_task( kmap::Uuid const& task )
        -> bool
    {
        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->is_task( task );
    }
};

auto task_store()
    -> ::TaskStore
{
    return ::TaskStore{ kmap::Singleton::instance() };
}

} // namespace anonymous

using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_task_store )
{
    function( "task_store", &::task_store );

    class_< ::TaskStore >( "TaskStore" )
        .function( "cascade_tags", &::TaskStore::cascade_tags )
        .function( "create_task", &::TaskStore::create_task )
        .function( "create_subtask", &::TaskStore::create_subtask )
        .function( "close_task", &::TaskStore::close_task )
        .function( "activate_task", &::TaskStore::activate_task )
        .function( "deactivate_task", &::TaskStore::deactivate_task )
        .function( "is_task", &::TaskStore::is_task )
        ;
}