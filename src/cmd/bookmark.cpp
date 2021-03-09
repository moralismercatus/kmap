/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "bookmark.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

namespace kmap::cmd {

namespace create_bookmark_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
let rv = null;
const bm_root = kmap.fetch_node( '.root.meta.bookmarks' );

if( bm_root.has_value() )
{
    const selected = kmap.selected_node();

    if( !kmap.is_alias_pair( selected, bm_root.value() ) )
    {
        rv = kmap.success( 'success' );
    }
    else
    {
        rv = kmap.failure( 'bookmark already exists' );
    }
}
else
{
    rv = kmap.success( 'success' );
}

return rv;
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const bm_root = kmap.fetch_or_create_node( kmap.root_node(), 'meta.bookmarks' );

if( bm_root.has_value() )
{
    const selected = kmap.selected_node();
    const alias = kmap.create_alias( selected, bm_root.value() );

    if( alias.has_value() )
    {
        rv = kmap.success( 'bookmark created' );
    }
    else
    {
        rv = kmap.failure( alias.error_message() );
    }
}
else
{
    rv = kmap.failure( bm_root.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates a bookmark of selected node";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "nonbookmark"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    create.bookmark
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace create_bookmark_def

namespace bookmark_heading_def {
namespace {

auto const guard_code =
R"%%%(```javascript
return kmap.is_valid_heading_path( arg );
```)%%%";
auto const completion_code =
R"%%%(```javascript
let rv = new kmap.VectorString();
const bm_root = kmap.fetch_node( '.root.meta.bookmarks' );

if( bm_root.has_value() )
{
    if( arg.length === 0 )
    {
        const children = kmap.fetch_children( bm_root.value() );

        rv = kmap.map_headings( children );
    }
    else
    {
        rv = kmap.complete_child_heading( bm_root.value(), arg );
    }
}

return rv;
```)%%%";

auto const description = "bookmark heading";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    bookmark_heading
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace bookmark_heading_def

namespace select_bookmark_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;
const bm_root = kmap.fetch_node( '.root.meta.bookmarks' );

if( bm_root.has_value() )
{
    const bm = kmap.fetch_child( bm_root.value(), args.get( 0 ) );

    if( bm.has_value() )
    {
        const dst = kmap.resolve_alias( bm.value() );

        kmap.select_node( dst );

        rv = kmap.success( 'bookmark selected' );
    }
    else
    {
        rv = kmap.failure( bm.error_message() );
    }
}
else
{
    rv = kmap.failure( bm_root.error_message() );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates a bookmark of selected node";
auto const arguments = std::vector< Argument >{ Argument{ "bookmark_heading"
                                                        , "heading of bookmark"
                                                        , "bookmark_heading" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    select.bookmark
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace select_bookmark_def

} // namespace kmap::cmd