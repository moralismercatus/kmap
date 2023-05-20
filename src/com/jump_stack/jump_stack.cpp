/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "jump_stack.hpp"

#include "emcc_bindings.hpp"
#include "kmap.hpp"
#include "util/result.hpp"
#include <com/canvas/canvas.hpp>
#include <com/network/network.hpp>
#include <contract.hpp>
#include <js_iface.hpp>
#include <path/act/order.hpp>
#include <test/util.hpp>

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
const payload = estore.fetch_payload(); payload.throw_on_error();
const from = kmap.uuid_from_string( payload.value().at( 'from_node' ).as_string() ); from.throw_on_error();
const to = kmap.uuid_from_string( payload.value().at( 'to_node' ).as_string() ); to.throw_on_error();
kmap.jump_stack().push_transition( from.value(), to.value() ).throw_on_error();
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
kmap.jump_stack().jump_out().throw_on_error();
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
kmap.jump_stack().jump_in().throw_on_error();
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
        auto const guard_code =
        R"%%%(
            return kmap.success( 'unconditional' );
        )%%%";
        auto const action_code =
        R"%%%(
            kmap.jump_stack().jump_out().throw_on_error();

            return kmap.success( 'success' );
        )%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "selects to jump_stack active node";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "jump.out"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
                                    , .action = action_code };

        KTRY( cclerk_.register_command( command ) );
    }
    // jump.in
    {
        auto const guard_code =
        R"%%%(
            return kmap.success( 'unconditional' );
        )%%%";
        auto const action_code =
        R"%%%(
            kmap.jump_stack().jump_in().throw_on_error();

            return kmap.success( 'success' );
        )%%%";

        using Guard = com::Command::Guard;
        using Argument = com::Command::Argument;

        auto const description = "jumps to top of jump stack";
        auto const arguments = std::vector< Argument >{};
        auto const guard = Guard{ "unconditional", guard_code };
        auto const command = Command{ .path = "jump.in"
                                    , .description = description
                                    , .arguments = arguments
                                    , .guard = guard
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

    KTRY( js::eval_void( 
R"%%%(
const canvas = kmap.canvas();
const js_pane_id = kmap.uuid_to_string( canvas.jump_stack_pane() ); js_pane_id.throw_on_error();
const tbl = document.getElementById( js_pane_id.value() );
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

auto is_parent( com::Network const& nw
              , Uuid const& parent
              , Uuid const& child )
    -> bool
{
    return KTRYB( nw.fetch_parent( child ) ) == parent;
}

auto is_sibling_adjacent( Kmap const& km
                        , Uuid const& node
                        , Uuid const& other )
    -> bool
{
    // Ensure nodes are siblings.
    {
        auto const nw = KTRYE( km.fetch_component< com::Network >() );
        auto const np = KTRYB( nw->fetch_parent( node ) );
        auto const op = KTRYB( nw->fetch_parent( other ) );

        KENSURE_B( np == op );
    }
    
    auto const siblings = anchor::node( node )
                        | view2::sibling_incl
                        | act2::to_node_set( km )
                        | act::order( km );
    auto const it1 = ranges::find( siblings, node );
    auto const it2 = ranges::find( siblings, other );

    if( it1 < it2 )
    {
        return std::distance( it1, it2 ) == 1;
    }
    else
    {
        return std::distance( it2, it1 ) == 1; 
    }
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

auto JumpStack::is_adjacent( Uuid const& n1
                           , Uuid const& n2 )
    -> bool 
{
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

    auto const nw = KTRY( fetch_component< com::Network >() );

    KMAP_ENSURE_BOOL( active_item_index_ );

    auto& active_index = active_item_index_.value();
    auto const selected = nw->selected_node();

    KMAP_ENSURE_BOOL( active_index != 0);

    --active_index;

    auto const prev = buffer_.at( active_index );

    buffer_.at( active_index ) = selected;

    ignore_transitions_ = true;
    KTRY( nw->select_node( prev ) );
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

    auto const nw = KTRY( fetch_component< com::Network >() );

    if( !active_item_index_ )
    {
        active_item_index_ = 0;
    }

    auto& active_index = active_item_index_.value();

    KMAP_ENSURE_BOOL( active_index < buffer_.size() );

    auto const selected = nw->selected_node();
    auto const next = buffer_.at( active_index );

    buffer_[ active_index ] = selected;

    ++active_index;

    ignore_transitions_ = true;
    KTRY( nw->select_node( next ) );
    ignore_transitions_ = false;

    KTRY( update_pane() );

    rv = true;

    return rv;
}

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

            REQUIRE( jstack->active_item_index() );
            REQUIRE( jstack->active_item_index().value() == 0 );
            REQUIRE( jstack->stack().at( 0 ) == root );
            REQUIRE( !REQUIRE_TRY( jstack->jump_in() ) );

            WHEN( ":jump.out" )
            {
                REQUIRE_TRY( jstack->jump_out() );

                REQUIRE( nw->selected_node() == root );
                REQUIRE( jstack->active_item_index() );
                REQUIRE( jstack->active_item_index().value() == 1 );
                REQUIRE( jstack->stack().size() == 1 );
                REQUIRE( jstack->stack().at( 0 ) == n1 );

                REQUIRE( REQUIRE_TRY( jstack->jump_out() ) == false );

                WHEN( "jump.in" )
                {
                    REQUIRE_TRY( jstack->jump_in() );

                    REQUIRE( nw->selected_node() == n1 );
                    REQUIRE( jstack->active_item_index() );
                    REQUIRE( jstack->active_item_index().value() == 0 );
                    REQUIRE( jstack->stack().size() == 1 );
                    REQUIRE( jstack->stack().at( 0 ) == root );
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

    KMAP_ENSURE_BOOL( !ignore_transitions_ ); // ignore push
    KMAP_ENSURE_BOOL( !is_adjacent( from, to ) ); // adjacent push

    clear_jump_in_items();

    KMAP_ENSURE_BOOL( buffer_.empty() || ( buffer_.front() != from ) ); // duplicate push

    if( buffer_.size() >= threshold() )
    {
        buffer_.pop_back();
    }

    buffer_.push_front( from );

    active_item_index_ = 0;

    KTRY( update_pane() );

    rv = true;

    return rv;
}

SCENARIO( "JumpStack::push_transition", "[jump_stack]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "jump_stack" );

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
        ,       { 4, REQUIRE_TRY( anchor::abs_root | view2::child( "2" ) | view2::child( "4" ) | act2::create_node( km ) ) }
        ,   { 3, REQUIRE_TRY( anchor::abs_root | view2::child( "3" ) | act2::create_node( km ) ) }
        };
        auto const check = [ & ]( auto const& from, auto const& to, std::vector< unsigned > const& istack ) -> bool
        {
            REQUIRE_TRY( jstack->push_transition( num_to_node_map.at( from ), num_to_node_map.at( to ) ) );

            auto const tstack = istack
                                | rvs::transform( [ & ]( auto const& e ){ return num_to_node_map.at( e ); } )
                                | ranges::to< std::deque< Uuid > >();


            return jstack->stack() == tstack;
        };

        THEN( "Only non-adjacent are pushed" )
        {
            REQUIRE( check( 1, 1, {} ) );
            REQUIRE( check( 1, 2, {} ) );
            REQUIRE( check( 1, 3, { 1 } ) );
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
                    REQUIRE( jstack->stack().front() == num_to_node_map.at( 1 ) );
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
                    REQUIRE( jstack->stack().front() == num_to_node_map.at( 4 ) );
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

    KTRY( js::eval_void( 
R"%%%(
const canvas = kmap.canvas();
const js_pane_id = kmap.uuid_to_string( canvas.jump_stack_pane() ); js_pane_id.throw_on_error();
const tbl = document.getElementById( js_pane_id.value() );
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
        const label = kmap.visnetwork().format_node_label( node );

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
)%%%" ) );

    rv = outcome::success();

    return rv;
}

namespace {
namespace jump_stack_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::JumpStack
,   std::set({ "root_node"s, "event_store"s, "canvas.workspace"s, "option_store"s, "command_store"s, "network"s })
,   "maintains non-adjacent node selection history"
);

} // namespace jump_stack_def 

namespace binding
{
    using namespace emscripten;

    struct JumpStack
    {
        Kmap& km_;

        auto active_item_index() const
            -> kmap::com::JumpStack::Stack::size_type
        {
            auto const jstack = KTRYE( km_.fetch_component< com::JumpStack >() );

            return jstack->active_item_index().value_or( 0 );
        }
        auto jump_in()
            -> kmap::binding::Result< void >
        {
            KM_RESULT_PROLOG();

            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            KTRY( jstack->jump_in() );

            rv = outcome::success();

            return rv;
        }
        auto jump_out()
            -> kmap::binding::Result< void >
        {
            KM_RESULT_PROLOG();

            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            KTRY( jstack->jump_out() );

            rv = outcome::success();

            return rv;
        }
        auto push_transition( Uuid const& from
                            , Uuid const& to )
            -> kmap::binding::Result< void >
        {
            KM_RESULT_PROLOG();
                KM_RESULT_PUSH_NODE( "from", from );
                KM_RESULT_PUSH_NODE( "to", to );

            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            KTRY( jstack->push_transition( from, to ) );

            rv = outcome::success();

            return rv;
        }
        auto set_threshold( unsigned const& max )
            -> void
        {
            auto const jstack = KTRYE( km_.fetch_component< com::JumpStack >() );

            jstack->threshold( max );
        }
        auto stack() const
            -> std::vector< Uuid >
        {
            auto const jstack = KTRYE( km_.fetch_component< com::JumpStack >() );

            return jstack->stack() | ranges::to< std::vector >();
        }
    };

    auto jump_stack()
        -> binding::JumpStack
    {
        return binding::JumpStack{ kmap::Singleton::instance() };
    }

    EMSCRIPTEN_BINDINGS( kmap_jump_stack )
    {
        function( "jump_stack", &kmap::com::binding::jump_stack );
        class_< kmap::com::binding::JumpStack >( "JumpStack" )
            .function( "active_item_index", &kmap::com::binding::JumpStack::active_item_index )
            .function( "jump_in", &kmap::com::binding::JumpStack::jump_in )
            .function( "jump_out", &kmap::com::binding::JumpStack::jump_out )
            .function( "push_transition", &kmap::com::binding::JumpStack::push_transition )
            .function( "set_threshold", &kmap::com::binding::JumpStack::set_threshold )
            .function( "stack", &kmap::com::binding::JumpStack::stack )
            ;
    }
} // namespace binding
} // namespace anon

} // namespace kmap::com
