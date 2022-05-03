/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cli.hpp"
#include "command.hpp"
#include "kmap.hpp"

#include <vector>

namespace kmap::cmd {

namespace {

namespace cmd::run_unit_tests {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
const res = kmap.run_unit_tests( args.get( 0 ) );

if( res.has_value() )
{
    return kmap.success( "all test cases executed successfully" );
}
else
{
    return kmap.failure( res.error_message() );
}
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "runs all build-in unit tests";
auto const arguments = std::vector< Argument >{ Argument{ "text" // TODO: unit test specific arg
                                                        , "tc, suite, or '*' for all tcs"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    run.unit_tests
,   description 
,   arguments
,   guard
,   action
);

} // namespace cmd::run_unit_tests

} // namespace anon

} // namespace kmap::cmd