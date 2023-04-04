/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "jump_stack.hpp"

#include "emcc_bindings.hpp"
#include "kmap.hpp"
#include "util/result.hpp"
#include <com/network/network.hpp>
#include <js_iface.hpp>
#include <path/act/order.hpp>

namespace kmap::com {

JumpStack::JumpStack( Kmap& km
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ km, requisites, description }
    , eclerk_{ km }
    , oclerk_{ km }
{
    KTRYE( register_event_outlets() );
    KTRYE( register_standard_options() );
}

auto JumpStack::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    KTRY( eclerk_.install_registered() );
    KTRY( oclerk_.install_registered() );
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
    KTRY( oclerk_.check_registered() );

    // KTRY( build_pane_table() );

    rv = outcome::success();

    return rv;
}

auto JumpStack::register_event_outlets()
    -> Result< void >
{
    auto rv = result::make_result< void >();

    {
        auto const action =
R"%%%(
const estore = kmap.event_store();
const payload = estore.fetch_payload(); payload.throw_on_error();
const from = kmap.uuid_from_string( payload.value().at( 'from_node' ).as_string() ); from.throw_on_error();
const to = kmap.uuid_from_string( payload.value().at( 'to_node' ).as_string() ); to.throw_on_error();
kmap.jump_stack().push_transition( from.value(), to.value() );
)%%%";
        eclerk_.register_outlet( Leaf{ .heading = "jump_stack.node_selected"
                                    , .requisites = { "subject.network", "verb.selected", "object.node" }
                                    , .description = "pushes non-adjacent node selections into the jump stack"
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

for( let i = 0; i < 10; ++i )
{
    const row = document.createElement( "tr" );
    const cell = document.createElement( "td" );

    row.backgroundColor = '#111111';
    cell.style.textAlign = "center";
    // cell.style.borderRadius = "5%";
    cell.style.color = 'white';
    cell.innerText = ( i ).toString();

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
    if( auto const& cp = nw.fetch_parent( child )
      ; cp )
    {
        return cp.value() == parent;
    }

    return false;
}

auto is_sibling_adjacent( Kmap const& km
                        , Uuid const& node
                        , Uuid const& other )
    -> bool
{
    // Ensure nodes are siblings.
    {
        auto const nw = KTRYE( km.fetch_component< com::Network >() );
        auto const np = nw->fetch_parent( node );
        auto const op = nw->fetch_parent( other );
        KENSURE_B( np && op && np.value() == op.value() );
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

auto JumpStack::is_adjacent( Uuid const& n1
                           , Uuid const& n2 )
    -> bool 
{
    KENSURE_B( !buffer_.empty() );

    // TODO: Rather than using "top", need to store a "prev_selected" value to use, otherwise travel.right, travel.right will result in a non-adjancy;
    //       therefore, I will need to store a "last selected" and use that.
    auto const nw = KTRYE( fetch_component< com::Network >() );

    return n1 == n2 
        || is_parent( *nw, n1, n2 )
        || is_parent( *nw, n2, n1 )
        || is_sibling_adjacent( kmap_inst(), n1, n2 );
}

auto JumpStack::push_transition( Uuid const& from
                               , Uuid const& to )
    -> void
{
    // TODO: Also needs to ensure entry is not followed by itself. How is that possible?
    //       Not sure... maybe it occurs because the initial root is pushed. Otherwise it shouldn't be possible.
    //       Maybe initial JS should be empty, not init'd with Root.
    if( !is_adjacent( from, to ) )
    {
        if( buffer_.size() > 99 )
        {
            buffer_.pop_back();
        }

        buffer_.push_front( from );

        KTRYE( js::eval_void( 
R"%%%(
const canvas = kmap.canvas();
const js_pane_id = kmap.uuid_to_string( canvas.jump_stack_pane() ); js_pane_id.throw_on_error();
const tbl = document.getElementById( js_pane_id.value() );
const tbody = tbl.firstChild;
const jstack = kmap.jump_stack();
const stack = jstack.stack();
const nw = kmap.network();

for( let i = 0
   ; i < tbody.children.length
   ; ++i )
{
    const cell = tbody.children[ i ].firstChild;
    
    if( i < stack.size() )
    {
        const node = stack.get( i );
        const label = kmap.visnetwork().format_node_label( node );

        cell.innerHTML = label;
    }
    else
    {
        cell.innerHTML = "";
    }
}
)%%%" ) );
    }
}

auto JumpStack::jump_out()
    -> Optional< Uuid >
{
    KMAP_THROW_EXCEPTION_MSG( "TODO" );
}

auto JumpStack::set_threshold( unsigned const& max )
    -> void
{
    threshold_ = max;
}

auto JumpStack::stack() const
    -> Stack const&
{
    return buffer_;
}

namespace {
namespace jump_stack_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::JumpStack
,   std::set({ "root_node"s, "event_store"s, "canvas"s, "option_store"s })
,   "maintains non-adjacent node selection history"
);

} // namespace jump_stack_def 

namespace binding
{
    using namespace emscripten;

    struct JumpStack
    {
        Kmap& km_;

        auto push_transition( Uuid const& from
                            , Uuid const& to )
            -> kmap::binding::Result< void >
        {
            KM_RESULT_PROLOG();
                KM_RESULT_PUSH_NODE( "from", from );
                KM_RESULT_PUSH_NODE( "to", to );

            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            jstack->push_transition( from, to );

            return rv;
        }
        auto jump_out()
            -> kmap::binding::Result< void >
        {
            KM_RESULT_PROLOG();

            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            jstack->jump_out();

            return rv;
        }
        auto set_threshold( unsigned const& max )
            -> void
        {
            auto const jstack = KTRYE( km_.fetch_component< com::JumpStack >() );

            jstack->set_threshold( max );
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
            .function( "push_transition", &kmap::com::binding::JumpStack::push_transition )
            .function( "jump_out", &kmap::com::binding::JumpStack::jump_out )
            .function( "set_threshold", &kmap::com::binding::JumpStack::set_threshold )
            .function( "stack", &kmap::com::binding::JumpStack::stack )
            ;
    }
} // namespace binding
} // namespace anon

} // namespace kmap::com
