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
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>

namespace kmap::cmd {

namespace {
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

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "TODO";
auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                        , "task title"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
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

SCENARIO( "create_task" , "[cmd][task]" )
{
    KMAP_COMMAND_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();
    auto& cli = kmap.cli();

    GIVEN( "default state" )
    {
        WHEN( "create.task" )
        {
            REQUIRE_RES( cli.parse_raw( ":create.task Victor Charlie Delta" ) );
        }
    }
}

namespace close_task_def {
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

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "TODO";
auto const arguments = std::vector< Argument >{ Argument{ "task_title"
                                                        , "task title"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
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

} // namespace anon
} // namespace kmap::cmd