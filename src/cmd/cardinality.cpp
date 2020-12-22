/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "cardinality.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

namespace kmap::cmd {

namespace count_ancestors_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const sel = kmap.selected_node();
const des = kmap.count_ancestors( sel );

if( des.has_value() )
{
    rv = kmap.success( des.value().toString() );
}
else
{
    rv = kmap.failure( des.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "displays descendants of selected";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    count.ancestors
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace count_ancestors_def

namespace count_descendants_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const sel = kmap.selected_node();
const des = kmap.count_descendants( sel );

if( des.has_value() )
{
    rv = kmap.success( des.value().toString() );
}
else
{
    rv = kmap.failure( des.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "displays descendants of selected";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    count.descendants
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace count_descendants_def

} // namespace kmap::cmd