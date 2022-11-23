/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "task.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"
#include "com/cmd/command.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>

namespace kmap::cmd {

namespace {

#if 0
auto const is_task_guard_code =
R"%%%(```javascript
if( kmap.task_store().is_task( nw->selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'not task' );
}
```)%%%";

namespace create_task_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
const taskn = kmap.task_store().create_task( args.get( 0 ) );

if( taskn.has_value() )
{
    kmap.select_node( taskn.value() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( taskn.error_message() );
}
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "creates task";
auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                        , "task title"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.task 
,   description 
,   arguments
,   guard
,   action
);

} // namespace create_task_def 
#endif // 0

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

#if 0
namespace cascade_tags_def {
auto const action_code =
R"%%%(```javascript
const closed = kmap.task_store().cascade_tags( nw->selected_node() );

if( closed.has_value() )
{
    kmap.select_node( kmap.selected_node() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( closed.error_message() );
}
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Applies supertask tags to subtasks";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_task", is_task_guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    cascade.tags 
,   description 
,   arguments
,   guard
,   action
);

} // namespace cascade_tags_def 
#endif // 0

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

                auto const tagn = REQUIRE_TRY( view::abs_root
                                             | view::direct_desc( "meta.tag.cat" )
                                             | view::fetch_node( kmap ) );
                REQUIRE(( view::abs_root
                        | view::direct_desc( "task.super.subtask.sub" )
                        | view::tag( tagn )
                        | view::exists( kmap ) ));
            }
        }
    }
}

#if 0
namespace close_task_def {
auto const action_code =
R"%%%(```javascript
const closed = kmap.task_store().close_task( kmap.selected_node() );

if( closed.has_value() )
{
    kmap.select_node( kmap.selected_node() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( closed.error_message() );
}
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Removes status.open tag and appends status.closed tag";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_task", is_task_guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    close.task 
,   description 
,   arguments
,   guard
,   action
);

} // namespace close_task_def 
#endif // 0

#if 0
namespace open_task_def {
auto const action_code =
R"%%%(```javascript
const opened = kmap.task_store().activate_task( kmap.selected_node() );

if( opened.has_value() )
{
    kmap.select_node( kmap.selected_node() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( opened.error_message() );
}
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Removes status.close tag and appends status.open tag";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_task", is_task_guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    open.task 
,   description 
,   arguments
,   guard
,   action
);

} // namespace open_task_def 
#endif // 0

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