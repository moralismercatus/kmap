/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/jump_stack/jump_stack.hpp>

#include <com/canvas/canvas.hpp>
#include <com/network/network.hpp>
#include <com/visnetwork/visnetwork.hpp>
#include <contract.hpp>
#include <kmap.hpp>
#include <path.hpp>
#include <path/act/order.hpp>
#include <path/disambiguate.hpp>
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

JumpStack::JumpStack( Kmap& km
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ km, requisites, description }
    , eclerk_{ km, { JumpStack::id } }
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

auto JumpStack::initialize()
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

auto JumpStack::load()
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

auto JumpStack::register_event_outlets()
    -> Result< void >
{
    auto rv = result::make_result< void >();

    // push transition
    {
        auto const action =
R"%%%(
const estore = kmap.event_store();
const payload = ktry( estore.fetch_payload() );
const from = ktry( kmap.uuid_from_string( payload.at( 'from_node' ).as_string() ) );
const to = ktry( kmap.uuid_from_string( payload.at( 'to_node' ).as_string() ) );
ktry( kmap.jump_stack().push_transition( from, to ) );
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "jump_stack.node_selected"
                                     , .requisites = { "subject.network", "verb.selected", "object.node" }
                                     , .description = "pushes non-adjacent node selections into the jump stack"
                                     , .action = action } );
    }
    // jump branch
    {
        eclerk_.register_outlet( Branch{ .heading = "jump_stack.key.control"
                                       , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.control" } } );
    }
    // jump.out
    {
        auto const action =
R"%%%(
ktry( kmap.jump_stack().jump_out() );
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "jump_stack.key.control.jump_out"
                                     , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.o" }
                                     , .description = "selects current jump stack node"
                                     , .action = action } );
    }
    // jump.in
    {
        auto const action =
R"%%%(
ktry( kmap.jump_stack().jump_in() );
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "jump_stack.key.control.jump_in"
                                     , .requisites = { "subject.network", "verb.depressed", "object.keyboard.key.i" }
                                     , .description = "selects previously jumped out-of node"
                                     , .action = action } );
    }

    rv = outcome::success();

    return rv;
}

auto JumpStack::register_standard_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( oclerk_.register_option( Option{ .heading = "jump_stack.threshold"
                                         , .descr = "Sets the maximum number of nodes stored."
                                         , .value = "100"
                                         , .action = R"%%%(kmap.jump_stack().set_threshold( option_value );)%%%" } ) );

    rv = outcome::success();

    return rv;
}

auto JumpStack::register_panes()
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

auto JumpStack::register_commands()
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


auto JumpStack::build_pane_table()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

#if !KMAP_NATIVE
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
#endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

auto JumpStack::active_item_index() const
    -> std::optional< Stack::size_type >
{
    return active_item_index_;
}

auto JumpStack::clear_jump_in_items()
    -> void
{
    if( active_item_index_ )
    {
        if( active_item_index_.value() > 0 )
        {
            auto const end_it = std::next( buffer_.begin(), active_item_index_.value() );

            buffer_.erase( buffer_.begin(), end_it );

            if( !buffer_.empty() )
            {
                active_item_index_ = 0;
            }
            else
            {
                active_item_index_ = std::nullopt;
            }
        }
    }
}

auto JumpStack::format_cell_label( Uuid const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = result::make_result< std::string >();
    auto const& km = kmap_inst();
    auto const dpath = KTRY( disambiguate_path3( km, node ) );
    auto const nl = KTRY( format_node_label( km, node ) );

    if( dpath.empty() )
    {
        rv = nl;
    }
    else
    {
        rv = fmt::format( "{}<br>{}", nl, dpath );
    }

    return rv;
}

auto JumpStack::is_adjacent( Uuid const& n1
                           , Uuid const& n2 )
    -> bool 
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( fetch_component< com::Network >() );

    return n1 == n2 
        || is_parent( *nw, n1, n2 )
        || is_parent( *nw, n2, n1 )
        || is_sibling_adjacent( kmap_inst(), n1, n2 );
}

auto JumpStack::jump_in()
    -> Result< bool >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< bool >();

    BC_CONTRACT()
        BC_POST([ &
                , old_index = BC_OLDOF( active_item_index_ ) ]
        {
            if( rv )
            {
                if( rv.value() )
                {
                    BC_ASSERT( old_index );
                    BC_ASSERT( active_item_index_.value() + 1 == old_index->value() );
                }
            }
        })
    ;

    purge_nonexistent();

    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE_BOOL( active_item_index_ );

    auto& active_index = active_item_index_.value();
    auto const selected = nw->selected_node();

    KMAP_ENSURE_BOOL( active_index != 0);

    --active_index;

    auto const prev = buffer_.at( active_index );
    auto const label = KTRY( format_cell_label( selected ) );

    buffer_.at( active_index ) = StackItem{ selected, label };

    ignore_transitions_ = true;
    KTRY( nw->select_node( prev.id ) );
    ignore_transitions_ = false;

    KTRY( update_pane() );

    rv = true;

    return rv;
}

auto JumpStack::jump_out()
    -> Result< bool >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< bool >();

    BC_CONTRACT()
        BC_POST([ &
                , old_index = BC_OLDOF( active_item_index_ ) ]
        {
            if( rv )
            {
                if( rv.value() )
                {
                    BC_ASSERT( active_item_index_ );

                    if( old_index )
                    {
                        BC_ASSERT( active_item_index_.value() == old_index->value() + 1 );
                    }
                    else
                    {
                        BC_ASSERT( active_item_index_.value() == 0 );
                    }
                }
            }
        })
    ;

    purge_nonexistent();

    auto const nw = KTRY( fetch_component< com::Network >() );

    if( !active_item_index_ )
    {
        active_item_index_ = 0;
    }

    auto& active_index = active_item_index_.value();

    KMAP_ENSURE_BOOL( active_index < buffer_.size() );

    auto const selected = nw->selected_node();
    auto const next = buffer_.at( active_index );
    auto const label = KTRY( format_cell_label( selected ) );

    buffer_[ active_index ] = StackItem{ selected, label };

    ++active_index;

    ignore_transitions_ = true;
    KTRY( nw->select_node( next.id ) );
    ignore_transitions_ = false;

    KTRY( update_pane() );

    rv = true;

    return rv;
}

// TODO: Are jump_in/out even used? I notice that outlet for node selected uses push_transition.
//       I think they are, but I'm not sure they _should be_. Maybe jump_out, but I imagine jump_in has been superceded fully by push_transition.
//       The contents of the test may properly carry over to push_transition, though. 
SCENARIO( "JumpStack::jump_in/out", "[jump_stack]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "jump_stack" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const jstack = REQUIRE_TRY( km.fetch_component< com::JumpStack >() );
    auto const root = nw->root_node();

    GIVEN( "/a.1" )
    {
        auto const n1 = REQUIRE_TRY(( anchor::abs_root | view2::direct_desc( "a.1" ) | act2::create_node( km ) ));

        REQUIRE( !jstack->active_item_index() );

        WHEN( ":@/a.1" )
        {
            REQUIRE_TRY( nw->select_node( n1 ) );
            
            #if KMAP_NATIVE
            REQUIRE_TRY( jstack->push_transition( root, n1 ) ); // Native omits JS-based events, so transition event fired by select_node won't be captured.
            #endif // KMAP_NATIVE

            REQUIRE( jstack->active_item_index() );
            REQUIRE( jstack->active_item_index().value() == 0 );
            REQUIRE( jstack->stack().at( 0 ).id == root );
            REQUIRE( !REQUIRE_TRY( jstack->jump_in() ) );

            WHEN( ":jump.out" )
            {
                REQUIRE_TRY( jstack->jump_out() );

                REQUIRE( nw->selected_node() == root );
                REQUIRE( jstack->active_item_index() );
                REQUIRE( jstack->active_item_index().value() == 1 );
                REQUIRE( jstack->stack().size() == 1 );
                REQUIRE( jstack->stack().at( 0 ).id == n1 );

                REQUIRE( REQUIRE_TRY( jstack->jump_out() ) == false );

                WHEN( "jump.in" )
                {
                    REQUIRE_TRY( jstack->jump_in() );

                    REQUIRE( nw->selected_node() == n1 );
                    REQUIRE( jstack->active_item_index() );
                    REQUIRE( jstack->active_item_index().value() == 0 );
                    REQUIRE( jstack->stack().size() == 1 );
                    REQUIRE( jstack->stack().at( 0 ).id == root );
                }
            }
        }

        GIVEN( "/b.2" )
        {
            // TODO:
            // auto const n2 = REQUIRE_TRY(( anchor::abs_root | view2::direct_desc( "b.2" ) | act2::create_node( km ) ));

            GIVEN( "/c.3" )
            {
                // TODO:
                // auto const n3 = REQUIRE_TRY(( anchor::abs_root | view2::direct_desc( "c.3" ) | act2::create_node( km ) ));
            }
        }
    }
}

auto JumpStack::purge_nonexistent()
    -> void
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( fetch_component< com::Network >() );
    auto& s = buffer_;

    for( auto it = s.begin()
       ; it != s.end()
       ; )
    {
        if( !nw->exists( it->id ) )
        {
            it = s.erase( it );
        }
        else
        {
            ++it;
        }
    }
}

auto JumpStack::push_transition( Uuid const& from
                               , Uuid const& to )
    -> Result< bool >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< bool >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( active_item_index_ );
                BC_ASSERT( active_item_index_.value() == 0 );
            }
        })
    ;

    purge_nonexistent();

    KMAP_ENSURE_BOOL( !ignore_transitions_ ); // ignore push
    KMAP_ENSURE_BOOL( !is_adjacent( from, to ) ); // adjacent push

    clear_jump_in_items();

    auto const from_label = KTRY( format_cell_label( from ) );

    KMAP_ENSURE_BOOL( buffer_.empty() || ( buffer_.front().id != from ) ); // duplicate push

    if( buffer_.size() >= threshold()
     && threshold() > 0 )
    {
        buffer_.pop_back();
    }

    buffer_.push_front( StackItem{ from, from_label } );

    active_item_index_ = 0;

    KTRY( update_pane() );

    rv = true;

    return rv;
}

SCENARIO( "JumpStack::push_transition", "[jump_stack]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "jump_stack", "network" );

    auto& km = Singleton::instance();
    auto const nw= REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const jstack = REQUIRE_TRY( km.fetch_component< com::JumpStack >() );

    // Adjacency.
    GIVEN( "/1,/2.4,/3" )
    {
        THEN( "no pushes => jstack empty" )
        {
            REQUIRE( jstack->stack().empty() );
            // TODO: visual stack is also empty
        }

        REQUIRE( jstack->threshold() > 5 );

        auto const num_to_node_map = std::map< unsigned, Uuid >
        {   
            { 1, REQUIRE_TRY( anchor::abs_root | view2::child( "1" ) | act2::create_node( km ) ) }
        ,   { 2, REQUIRE_TRY( anchor::abs_root | view2::child( "2" ) | act2::create_node( km ) ) }
        ,   { 4, REQUIRE_TRY( anchor::abs_root | view2::child( "2" ) | view2::child( "4" ) | act2::create_node( km ) ) }
        ,   { 3, REQUIRE_TRY( anchor::abs_root | view2::child( "3" ) | act2::create_node( km ) ) }
        };
        auto const check = [ & ]( auto const& from, auto const& to, std::vector< unsigned > const& istack ) -> bool
        {
            REQUIRE_TRY( jstack->push_transition( num_to_node_map.at( from ), num_to_node_map.at( to ) ) );

            auto const tistack = istack
                               | rvs::transform( [ & ]( auto const& e ){ return num_to_node_map.at( e ); } )
                               | ranges::to< std::deque< Uuid > >();
            auto const tjstack = jstack->stack()
                               | rvs::transform( [ & ]( auto const& e ){ return e.id; } )
                               | ranges::to< std::deque< Uuid > >();

            // fmt::print( "{} == {}\n", jstack->stack().size(), tstack.size() );

            return tjstack == tistack;
        };

        THEN( "Only non-adjacent are pushed" )
        {
            REQUIRE( check( 1, 1, {} ) );
            REQUIRE( check( 1, 2, {} ) );
            REQUIRE( check( 1, 3, { 1 } ) ); // Non-adjacent, but repeat
            REQUIRE( check( 1, 4, { 1 } ) ); // Non-adjacent, but repeat
            REQUIRE( check( 2, 1, { 1 } ) );
            REQUIRE( check( 2, 2, { 1 } ) );
            REQUIRE( check( 2, 3, { 1 } ) );
            REQUIRE( check( 2, 4, { 1 } ) );
            REQUIRE( check( 3, 1, { 3, 1 } ) );
            REQUIRE( check( 3, 2, { 3, 1 } ) );
            REQUIRE( check( 3, 3, { 3, 1 } ) );
            REQUIRE( check( 3, 4, { 3, 1, } ) ); // Non-adjacent, but repeat
            REQUIRE( check( 4, 1, { 4, 3, 1 } ) );
            REQUIRE( check( 4, 2, { 4, 3, 1 } ) );
            REQUIRE( check( 4, 3, { 4, 3, 1 } ) ); // Non-adjacent, but repeat
            REQUIRE( check( 4, 4, { 4, 3, 1 } ) );
        }
        // Threshold condition.
        GIVEN( "threshold( 1 )" )
        {
            jstack->threshold( 1 );

            WHEN( "non-adjacent first push" )
            {
                REQUIRE_TRY( jstack->push_transition( num_to_node_map.at( 1 ), num_to_node_map.at( 3 ) ) );

                THEN( "stack-size: 1")
                {
                    REQUIRE( jstack->stack().size() == 1 );
                }
                THEN( "stack.top == first push" )
                {
                    REQUIRE( jstack->stack().front().id == num_to_node_map.at( 1 ) );
                }

                WHEN( "non-adjacement second push" )
                {
                    REQUIRE_TRY( jstack->push_transition( num_to_node_map.at( 4 ), num_to_node_map.at( 1 ) ) );

                    THEN( "stack-size == 1" )
                    {
                        REQUIRE( jstack->stack().size() == 1 );
                    }
                    THEN( "stack.top == most recent push" )
                    {
                    REQUIRE( jstack->stack().front().id == num_to_node_map.at( 4 ) );
                    }
                }
            }
        }
        // Disallow/Ignore repeat pushes.
        GIVEN( "push( 1, 3 )" )
        {
            REQUIRE( check( 1, 3, { 1 } ) );

            WHEN( "push( 1, 3 ) ")
            {
                THEN( "nothing pushed; repeated pushes disallowed" )
                {
                    REQUIRE( check( 1, 3, { 1 } ) );
                }
            }
        }
    }
}

auto JumpStack::threshold( Stack::size_type const& max )
    -> void
{
    threshold_ = max;
}

auto JumpStack::threshold() const
    -> unsigned
{
    return threshold_;
}

auto JumpStack::stack() const
    -> Stack const&
{
    return buffer_;
}

auto JumpStack::update_pane()
    -> Result< void > 
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
#if !KMAP_NATIVE
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
#endif // !KMAP_NATIVE

    rv = outcome::success();

    return rv;
}

namespace {
namespace jump_stack_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::JumpStack
,   std::set({ "root_node"s, "event_store"s, "canvas.workspace"s, "option_store"s, "command.store"s, "command.standard_items"s, "network"s })
,   "maintains non-adjacent node selection history"
);

} // namespace jump_stack_def 
} // namespace anon
} // namespace kmap::com
