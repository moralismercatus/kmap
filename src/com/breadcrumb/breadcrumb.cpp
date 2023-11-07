/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/breadcrumb/breadcrumb.hpp>

#include <com/canvas/canvas.hpp>
#include <com/network/network.hpp>
#include <com/visnetwork/visnetwork.hpp>
#include <contract.hpp>
#include <emcc_bindings.hpp>
#include <kmap.hpp>
#include <path.hpp>
#include <path/act/order.hpp>
#include <test/util.hpp>
#include <util/result.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::com {

Breadcrumb::Breadcrumb( Kmap& km
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ km, requisites, description }
    , eclerk_{ km, { Breadcrumb::id } }
    , oclerk_{ km }
    , pclerk_{ km }
    , cclerk_{ km }
{
    KM_RESULT_PROLOG();

    KTRYE( register_event_outlets() );
    KTRYE( register_standard_options() );
    KTRYE( register_panes() );
    KTRYE( register_commands() );
}

auto Breadcrumb::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( eclerk_.install_registered() );
    KTRY( pclerk_.install_registered() );
    KTRY( oclerk_.install_registered() );
    KTRY( cclerk_.install_registered() );
    // KTRY( oclerk_.apply_installed() );

    KTRY( build_pane_table() );

    rv = outcome::success();

    return rv;
}

auto Breadcrumb::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( eclerk_.check_registered() );
    KTRY( pclerk_.check_registered() );
    KTRY( oclerk_.check_registered() );
    KTRY( cclerk_.check_registered() );

    KTRY( build_pane_table() );

    rv = outcome::success();

    return rv;
}

auto Breadcrumb::register_event_outlets()
    -> Result< void >
{
    auto rv = result::make_result< void >();

    rv = outcome::success();

    return rv;
}

auto Breadcrumb::register_standard_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    // Hmm... maybe this shouldn't even be an option?
    // Can it, rather, simply be calculated? We can fit N number of cells given the pane size...
    // I think this is correct. What should really be an option is the max number of characters in the font per cell, and let calculations decide how many cells can fit.
    KTRY( oclerk_.register_option( Option{ .heading = "breadcrumb.threshold"
                                         , .descr = "Sets the maximum number of cells in the breadcrumb (cell => node)"
                                         , .value = "10"
                                         , .action = R"%%%(kmap.jump_stack().set_threshold( option_value );)%%%" } ) );

    rv = outcome::success();

    return rv;
}

auto Breadcrumb::register_panes()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( pclerk_.register_pane( Pane{ .id = jump_stack_uuid
                                     , .heading = "jump_stack"
                                     , .division = Division{ Orientation::vertical, 0.000f, false, "table" } } ) );

    rv = outcome::success();

    return rv;
}

auto Breadcrumb::register_commands()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    // jump.out
    {
        auto const action_code =
        R"%%%(
            ktry( kmap.jump_stack().jump_out() );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "selects to jump_stack active node";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "jump.out"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRY( cclerk_.register_command( command ) );
    }
    // jump.in
    {
        auto const action_code =
        R"%%%(
            ktry( kmap.jump_stack().jump_in() );
        )%%%";

        using Argument = com::Command::Argument;

        auto const description = "jumps to top of jump stack";
        auto const arguments = std::vector< Argument >{};
        auto const command = Command{ .path = "jump.in"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = "unconditional"
                                    , .action = action_code };

        KTRY( cclerk_.register_command( command ) );
    }

    rv = outcome::success();

    return rv;
}


auto Breadcrumb::build_pane_table()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( js::eval_void( 
R"%%%(
const canvas = kmap.canvas();
const js_pane_id = kmap.uuid_to_string( canvas.jump_stack_pane() );
const tbl = document.getElementById( js_pane_id );
const tbl_body = document.createElement( "tbody" );

tbl.style.backgroundColor = '#222222';
tbl.style.tableLayout = 'fixed';

for( let i = 0; i < 10; ++i )
{
    const row = document.createElement( "tr" );
    const cell = document.createElement( "td" );

    row.style.width="100%";
    row.style.height="10%";
    row.backgroundColor = '#111111';
    cell.style.textAlign = "center";
    // cell.style.borderRadius = "5%";
    cell.style.color = 'white';
    cell.style.overflow = 'hidden';
    cell.style.textOverflow = 'ellipsis';

    row.appendChild( cell );
    tbl_body.appendChild( row );
}

tbl.appendChild( tbl_body );
)%%%" ) );

    rv = outcome::success();

    return rv;
}

auto Breadcrumb::active_item_index() const
    -> std::optional< Stack::size_type >
{
    return active_item_index_;
}

auto Breadcrumb::update_pane()
    -> Result< void > 
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const raw_code = 
R"%%%(
const canvas = kmap.canvas();
const js_pane_id = kmap.uuid_to_string( canvas.jump_stack_pane() );
const tbl = document.getElementById( js_pane_id );
const tbody = tbl.firstChild;
const jstack = kmap.jump_stack();
const stack = jstack.stack();
const nw = kmap.network();
const active_index = jstack.active_item_index();

for( let i = 0
   ; i < tbody.children.length
   ; ++i )
{
    const row = tbody.children[ i ];
    const cell = row.firstChild;
    
    if( i < stack.size() )
    {
        const node = stack.get( i );
        const label = ktry( kmap.jump_stack().format_node_label( node ) );

        cell.innerHTML = label;

        if( kmap.network().is_alias( node ) )
        {
            console.log( 'setting alias special font for alias' );
            cell.style.fontFamily = 'monospace'; // TODO: This should be obtained from the usual alias-font described in options.
        }
        else
        {
            cell.style.fontFamily = ''; 
        }
    }
    else
    {
        cell.innerText = "";
    }

    if( i == active_index )
    {
        cell.style.borderColor = 'yellow'; // TODO: Get color from js state which can be updated via options mechanism.
    }
    else
    {
        cell.style.borderColor = 'white'; // TODO: Get color from js state which can be updated via options mechanism.
    }
}
)%%%";
    auto const pped = KTRY( js::preprocess( raw_code ) );

    KTRY( js::eval_void( pped ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::com
