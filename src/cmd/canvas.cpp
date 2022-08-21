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
#include "com/cmd/command.hpp"

namespace kmap::cmd {

#if 0
namespace canvas_pane_path {
namespace {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
auto const completion_code =
R"%%%(```javascript
return kmap.canvas().complete_path( arg );
```)%%%";

auto const description = "Path describing canvas pane";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    canvas.pane.path
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace heading_path 
#endif // 0

#if 0
namespace { 
namespace focus_pane_def {

auto const guard_code =
R"%%%(```javascript
    return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.failure( 'failed to focus pane' );
const canvas = kmap.canvas();
const pane = canvas.fetch_pane( args.get( 0 ) );

if( pane.has_value() ) // TODO: Is it possible to fail if canvas.pane.path holds true?
{
    const res = canvas.focus( pane.value() );

    if( !res.has_error() )
    {
        rv = kmap.success( 'pane focused' );
    }
}
else
{
    rv = kmap.failure( pane.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Sets target pane as the active pane";
auto const arguments = std::vector< Argument >{ Argument{ "pane"
                                                        , "path to target canvas pane"
                                                        , "canvas.pane.path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    focus.pane
,   description 
,   arguments
,   guard
,   action
);

} // namespace focus_pane_def
} // namespace anon
#endif // 0

#if 0
namespace hide_pane_def {
namespace {

auto const guard_code =
R"%%%(```javascript
    return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.failure( 'failed to hide pane' );
const canvas = kmap.canvas();
const pane = canvas.fetch_pane( args.get( 0 ) );

if( pane.has_value() ) // TODO: Is it possible to fail if canvas.pane.path holds true?
{
    const res = canvas.hide( pane.value() );

    if( !res.has_error() )
    {
        rv = kmap.success( 'pane hidden' );
    }
}
else
{
    rv = kmap.failure( pane.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Hides target canvas pane";
auto const arguments = std::vector< Argument >{ Argument{ "pane"
                                                        , "path to target canvas pane"
                                                        , "canvas.pane.path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    hide.pane
,   description 
,   arguments
,   guard
,   action
);

} // namespace hide_pane_def
} // namespace anon
#endif // 0

#if 0
namespace rebase_pane_def {
namespace {

auto const guard_code =
R"%%%(```javascript
    return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.failure( 'failed to rebase pane' );
const canvas = kmap.canvas();
const pane = canvas.fetch_pane( args.get( 0 ) );
const base = Number( args.get( 1 ) );

if( pane.has_value() ) // TODO: Is it possible to fail if canvas.pane.path holds true?
{
    const res = canvas.rebase( pane.value(), base );

    if( !res.has_error() )
    {
        rv = kmap.success( 'pane rebased' );
    }
}
else
{
    rv = kmap.failure( pane.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Reveals target canvas pane";
auto const arguments = std::vector< Argument >{ Argument{ "pane"
                                                        , "path to target canvas pane"
                                                        , "canvas.pane.path" }
                                              , Argument{ "percent"
                                                        , "percent of superpane for pane base"
                                                        , "numeric.decimal" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    rebase.pane
,   description 
,   arguments
,   guard
,   action
);

} // namespace rebase_pane_def
} // namespace anon
#endif // 0

#if 0
namespace reveal_pane_def {
namespace {

auto const guard_code =
R"%%%(```javascript
    return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.failure( 'failed to reveal pane' );
const canvas = kmap.canvas();
const pane = canvas.fetch_pane( args.get( 0 ) );

if( pane.has_value() ) // TODO: Is it possible to fail if arg guard canvas.pane.path holds true?
{
    const res = canvas.reveal( pane.value() );

    if( !res.has_error() )
    {
        rv = kmap.success( 'pane revealed' );
    }
}
else
{
    rv = kmap.failure( pane.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Reveals target canvas pane";
auto const arguments = std::vector< Argument >{ Argument{ "pane"
                                                        , "path to target canvas pane"
                                                        , "canvas.pane.path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    reveal.pane
,   description 
,   arguments
,   guard
,   action
);

} // namespace reveal_pane_def
} // namespace anon
#endif // 0

#if 0
namespace { 
namespace rotate_pane_def {

auto const guard_code =
R"%%%(```javascript
    return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = kmap.failure( 'failed to rotate pane' );
const canvas = kmap.canvas();
const pane = canvas.fetch_pane( args.get( 0 ) );

if( pane.has_value() ) // TODO: Is it possible to fail if canvas.pane.path holds true?
{
    const res = canvas.reorient( pane.value() );

    if( !res.has_error() )
    {
        rv = kmap.success( 'pane rotated' );
    }
}
else
{
    rv = kmap.failure( pane.error_message() );
}

return rv;
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "Reorients target pane";
auto const arguments = std::vector< Argument >{ Argument{ "pane"
                                                        , "path to target canvas pane"
                                                        , "canvas.pane.path" } };
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    rotate.pane
,   description 
,   arguments
,   guard
,   action
);

} // namespace rotate_pane_def
} // namespace anon
#endif // 0

} // namespace kmap::cmd