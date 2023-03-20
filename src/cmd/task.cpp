/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <cmd/task.hpp>

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "com/cmd/command.hpp"
#include "command.hpp"
#include "test/util.hpp"
#include <com/task/task.hpp>
#include <path/node_view.hpp>
#include <path/node_view2.hpp>

#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>

namespace kmap::cmd {

namespace {

SCENARIO( "cmd::create_task" , "[cmd][task]" )
{
    // TODO: I think the problem is in the CLI. Requires a Canvas to update, but it shouldn't. That should all be event sender/listener.
    // TODO: Component::load, rather than Component::initialize(). This will require some major work, I suspect, but same "requisites" principle apply.
    //       But first make everything work with "initialize()", COMPONENT_FIXTURE.
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store", "cli" );

    auto& kmap = Singleton::instance();
    auto const cli = REQUIRE_TRY( kmap.fetch_component< com::Cli >() );

    GIVEN( "default state" )
    {
        WHEN( "create.task" )
        {
            REQUIRE_RES( cli->parse_raw( ":create.task Victor Charlie Delta" ) );
        }
    }
}

SCENARIO( "cmd::activate_task", "[cmd][task]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store", "cli" );

    auto& kmap = Singleton::instance();
    auto const cli = REQUIRE_TRY( kmap.fetch_component< com::Cli >() );

    GIVEN( "create.task 1" )
    {
        REQUIRE_TRY( cli->parse_raw( ":create.task 1" ) );

        auto const t1 = REQUIRE_TRY( view2::task::task_root
                                   | view2::child( "1" )
                                   | act2::fetch_node( kmap ) );

        THEN( "task 1 in inactivate state" )
        {
            REQUIRE(( view::make( t1 )
                    | view::tag( "task.status.open.inactive" )
                    | view::exists( kmap ) ));
        }

        WHEN( "activate.task" )
        {
            REQUIRE_TRY( cli->parse_raw( ":activate.task" ) );

            THEN( "task 1 in activate state" )
            {
                REQUIRE(( view::make( t1 )
                        | view::tag( "task.status.open.active" )
                        | view::exists( kmap ) ));
            }
        }
    }
}

SCENARIO( "cmd::create_subtask" , "[cmd][task]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store", "cli" );

    auto& kmap = Singleton::instance();
    auto const cli = REQUIRE_TRY( kmap.fetch_component< com::Cli >() );

    GIVEN( "one supertask" )
    {
        REQUIRE_RES( cli->parse_raw( ":create.task Super" ) );

        GIVEN( "create.subtask" )
        {
            REQUIRE_RES( cli->parse_raw( ":create.subtask Subtask to Super" ) );
            // Rest of the testing is done in `TaskStore::create_subtask`.
        }
        GIVEN( "create.subtask from subtask" )
        {
            REQUIRE_RES( cli->parse_raw( ":create.subtask Subsubtask to subtask" ) );
        }
    }
}

SCENARIO( "cmd::cascade_tags" , "[cmd][task]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "task_store", "cli" );

    auto& kmap = Singleton::instance();
    auto const cli = REQUIRE_TRY( kmap.fetch_component< com::Cli >() );

    GIVEN( "super and sub task" )
    {
        REQUIRE_RES( cli->parse_raw( ":create.task Super" ) );
        REQUIRE_RES( cli->parse_raw( ":create.subtask Sub" ) );

        GIVEN( "supertask is tagged custom" )
        {
            REQUIRE_RES( cli->parse_raw( ":create.tag cat" ) );
            REQUIRE_RES( cli->parse_raw( ":@/task.super" ) );
            REQUIRE_RES( cli->parse_raw( ":tag.node cat" ) );

            WHEN( "cascade.tags" )
            {
                REQUIRE_RES( cli->parse_raw( ":cascade.tags" ) );

                auto const tagn = REQUIRE_TRY( anchor::abs_root
                                             | view2::direct_desc( "meta.tag.cat" )
                                             | act2::fetch_node( kmap ) );
                REQUIRE(( view::abs_root
                        | view::direct_desc( "task.super.subtask.sub" )
                        | view::tag( tagn )
                        | view::exists( kmap ) ));
            }
        }
    }
}

SCENARIO( "open.task", "[cmd][task][open]")
{
    GIVEN( "supertask" )
    {
        WHEN( "close" )
        {
            THEN( "fail" )
            {
                // TODO
            }
        }
        GIVEN( "body content" )
        {
            WHEN( "close" )
            {
                THEN( "fail" )
                {
                    // TODO
                }
            }
        }
        AND_GIVEN( "category tag" )
        {
            WHEN( "close" )
            {
                THEN( "task closed" )
                {
                    // TODO
                }
            }
        }
    }
    AND_GIVEN( "subtask" )
    {
    }
}

} // namespace anon
} // namespace kmap::cmd