/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>

namespace kmap::cmd {

namespace {

namespace tag_path {

auto const guard_code =
R"%%%(```javascript
return kmap.is_valid_heading_path( arg );
```)%%%";
auto const completion_code =
R"%%%(```javascript
const troot = kmap.fetch_node( "/meta.tag" );

if( troot.has_error() )
{
    return new kmap.VectorString();
}
else
{
    return kmap.complete_heading_path_from( troot.value(), troot.value(), arg );
}
```)%%%";

auto const description = "tag heading path";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    tag_path 
,   description 
,   guard
,   completion
);

} // namespace heading_path 

namespace create_tag_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
const tagn = kmap.tag_store().create_tag( args.get( 0 ) );

if( tagn.has_value() )
{
    kmap.select_node( tagn.value() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( kmap.error_message() );
}
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "Creates tag node";
auto const arguments = std::vector< Argument >{ Argument{ "tag_path"
                                                        , "heading path for tag node"
                                                        , "tag_path" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.tag
,   description 
,   arguments
,   guard
,   action
);
} // namespace create_tag_def 

namespace tag_node_def {
auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.tag_store().tag_node( kmap.selected_node(), args.get( 0 ) ).throw_on_error();

kmap.select_node( kmap.selected_node() );

return kmap.success( 'success' );
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "TODO";
auto const arguments = std::vector< Argument >{ Argument{ "tag_path"
                                                        , "path to target tag"
                                                        , "tag_path" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    tag.node
,   description 
,   arguments
,   guard
,   action
);
} // namespace tag_node_def 
} // namespace anon

} // namespace kmap::cmd