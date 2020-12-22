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
namespace create_contact_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
 // TODO: Requires same treatment as recipes, project, etc. for distinguishing categories.
 //       Current impl. just uses selected node as destination.
 //       Also, no error handling is performed!
auto const action_code =
R"%%%(```javascript
let dst = kmap.selected_node();
let contact = kmap.create_child( dst, args.get( 0 ) );
kmap.create_child( contact.value(), "Email" );
kmap.create_child( contact.value(), "Occupation" );
kmap.create_child( contact.value(), "Association" );
kmap.select_node( contact.value() );
return kmap.success( "contact '" + args.get( 0 ) + "' created" );
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "";
auto const arguments = std::vector< Argument >{ Argument{ "name"
                                                        , "Contact Name"
                                                        , "unconditional" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.contact
,   description 
,   arguments
,   guard
,   action
);

} // namespace create_contact_def

} // namespace anon

} // namespace kmap::cmd