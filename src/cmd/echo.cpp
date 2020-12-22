/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "echo.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>

namespace kmap::cmd {

namespace {
namespace echo_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
return kmap.success( args.get( 0 ) );
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "repeats argument to command bar";
auto const arguments = std::vector< Argument >{ Argument{ "text"
                                                        , "text to print"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    echo
,   description 
,   arguments
,   guard
,   action
);

} // namespace echo_def

// TODO: print.id probably belongs in print.cpp, or elsewhere than piggybacking on echo.cpp.
namespace print_id_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
return kmap.success( kmap.uuid_to_string( kmap.selected_node() ).value() );
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "prints selected node id to comamnd bar";
auto const arguments = std::vector< Argument >{};
auto const guard = PreregisteredCommand::Guard{ "unconditional"
                                              , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    print.id 
,   description 
,   arguments
,   guard
,   action
);

} // namespace print_id_def

namespace print_heading_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
return kmap.success( kmap.fetch_heading( kmap.selected_node() ).value() );
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "prints selected node heading to comamnd bar";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    print.heading 
,   description 
,   arguments
,   guard
,   action
);

} // namespace print_id_def

} // namespace anon

} // namespace kmap::cmd