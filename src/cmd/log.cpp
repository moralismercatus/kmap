/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "log.hpp"

#include "../cmd/project.hpp"
#include "../common.hpp"
#include "../contract.hpp"
#include "../emcc_bindings.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "../path.hpp"
#include "command.hpp"

#include <date/date.h>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/remove.hpp>

using namespace ranges;

namespace kmap::cmd::log {

auto fetch_or_create_daily_log( Kmap& kmap
                              , std::string const& date )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( kmap.fetch_or_create_descendant( kmap.root_node_id(), fmt::format( "/logs.daily.{}", date ) ) );

    return rv;
}

auto fetch_or_create_daily_log( Kmap& kmap )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( fetch_or_create_daily_log( kmap, to_log_date_string( std::chrono::system_clock::now() ) ) );

    return rv;
}

auto fetch_daily_log( Kmap const& kmap
                    , std::string const& date )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( kmap.fetch_descendant( fmt::format( "/logs.daily.{}", date ) ) );

    return rv;
}

auto fetch_daily_log( Kmap const& kmap )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( fetch_daily_log( kmap, to_log_date_string( std::chrono::system_clock::now() ) ) );

    return rv;
}

// Now we're getting into the realm of cross-domain commands (projects and daily logs), and the question of where to place the command comes into question.
// I'm adding it here because log is the thing changing whereas the project can be thought of as read only.
auto add_daily_task( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        auto const selected = kmap.selected_node();

        if( auto const project_root = fetch_project_root( kmap, selected ) )
        {
            if( auto const log = fetch_or_create_daily_log( kmap )
              ; log )
            {
                auto const task_root = kmap.fetch_or_create_descendant( log.value(), "tasks" );

                if( auto const task = kmap.create_alias( project_root.value()
                                                       , task_root.value() )
                ; task )
                {
                    KMAP_TRY( kmap.select_node( kmap.selected_node() ) );

                    return fmt::format( "added {} as a task of {}"
                                      , kmap.absolute_path_flat( project_root.value() )
                                      , kmap.absolute_path_flat( log.value() ) );
                }
                else
                {
                    return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "could not add task" );
                }
            }
            else
            {
                return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "could not obtain daily log" );
            }
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "selected node is not a project nor a descendant of" );
        }
    };
}

// TODO: Create unit test with other yy.mm.dd to ensure the correct is selected.
auto create_daily_log( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        if( auto const did = fetch_or_create_daily_log( kmap )
          ; did )
        {
            KMAP_TRY( kmap.select_node( did.value() ) );

            return fmt::format( "log created or was existent" );
        }
        else
        {
            return fmt::format( "unable to create daily log" );
        }
    };
}

namespace {

namespace create_daily_log_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const path = kmap.present_daily_log_path();
const ln = kmap.fetch_or_create_node( kmap.root_node(), path );

if( ln.has_value() )
{
    kmap.select_node( ln.value() );

    rv = kmap.success( 'log created or existent' );
}
else
{
    rv = kmap.failure( ln.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates a log for today's date";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.daily.log
,   description 
,   arguments
,   guard
,   action
);

} // namespace create_daily_log_def

namespace log_reference_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const path = kmap.present_daily_log_path();
const ln = kmap.fetch_or_create_node( kmap.root_node(), path );

if( ln.has_value() )
{
    const sel = kmap.selected_node();
    const ref = kmap.create_reference( sel, ln.value() );

    if( ref.has_value() )
    {
        rv = kmap.success( 'reference logged' );
    }
    else
    {
        rv = kmap.failure( ref.error_message() );
    }
}
else
{
    rv = kmap.failure( ln.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates alias for the selected node as reference of the daily log";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    log.reference
,   description 
,   arguments
,   guard
,   action
);

} // namespace log_reference_def

namespace log_task_def {

// TODO: Make unit test ensuring we alias the project root when selected is not the root, but a subnode of the proot.
// TODO: Represents a bit of a quandry. Where does it belong? Project or log? Probably project. Move it there.
auto const guard_code =
R"%%%(```javascript
if( kmap.is_in_project( kmap.resolve_alias( kmap.selected_node() ) ) )
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
const path = kmap.present_daily_log_path();
const ln = kmap.fetch_or_create_node( kmap.root_node(), path );

if( ln.has_value() )
{
    const sel = kmap.resolve_alias( kmap.selected_node() );
    const proj = kmap.fetch_project_root( sel ).value();
    const tasks = kmap.fetch_or_create_node( ln.value(), "/tasks" );

    if( tasks.has_value() )
    {
        const alias = kmap.create_alias( proj, tasks.value() );

        if( alias.has_value() )
        {
            rv = kmap.success( 'task logged' );
        }
        else
        {
            rv = kmap.failure( alias.error_message() );
        }
    }
    else
    {
        rv = kmap.failure( tasks.error_message() );
    }
}
else
{
    rv = kmap.failure( ln.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates alias for the selected project as task of the daily log";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_in_project"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    log.task
,   description 
,   arguments
,   guard
,   action
);

} // namespace log_task_def

namespace select_daily_log_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const path = kmap.present_daily_log_path();
const ln = kmap.fetch_node( path );

if( ln.has_value() )
{
    kmap.select_node( ln.value() );

    rv = kmap.success( 'today\'s log selected' );
}
else
{
    rv = kmap.failure( ln.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "selects the log for today's date";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    select.daily.log
,   description 
,   arguments
,   guard
,   action
);

} // namespace select_daily_log_def

} // namespace anon
namespace binding {

using namespace emscripten;

auto present_date_string()
    -> std::string
{
    // TODO: Awaiting C++20 chrono date capabilities.
    // return to_log_date_string( std::chrono::system_clock::now() );

    auto t = std::time( nullptr );
    auto ss = std::stringstream{};

    ss << std::put_time( std::localtime( &t ), "%Y.%m.%d" );

    return ss.str();
}

auto present_daily_log_path()
    -> std::string
{
    auto const pds = present_date_string();

    return fmt::format( "{}.{}"
                      , "/logs.daily" 
                      , pds );
}

EMSCRIPTEN_BINDINGS( kmap_module )
{
    function( "present_date_string", &kmap::cmd::log::binding::present_date_string );
    function( "present_daily_log_path", &kmap::cmd::log::binding::present_daily_log_path );
}

} // namespace binding

} // namespace kmap::cmd::log