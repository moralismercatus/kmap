/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/jump_stack/jump_stack.hpp>
#include <path.hpp>
#include <path/disambiguate.hpp>

#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::com {

namespace binding
{
    using namespace emscripten;

    struct JumpStack
    {
        Kmap& km_;

        auto active_item_index() const
            -> kmap::com::JumpStack::Stack::size_type
        {
            KM_RESULT_PROLOG();

            auto const jstack = KTRYE( km_.fetch_component< com::JumpStack >() );

            return jstack->active_item_index().value_or( 0 );
        }
        // TODO: format_cell_label (not even sure 'format' is right term...)
        auto format_node_label( Uuid const& node )
            -> Result< std::string >
        {
            KM_RESULT_PROLOG();
                KM_RESULT_PUSH( "node", node );

            auto rv = result::make_result< std::string >();
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );
            auto const& jss = jstack->stack();

            if( auto const it = ranges::find_if( jss, [ &node ]( auto const& e ){ return e.id == node; } )
              ; it != jss.end() )
            {
                rv = it->label;
            }

            return rv;
        }
        auto jump_in()
            -> kmap::Result< void >
        {
            KM_RESULT_PROLOG();

            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            KTRY( jstack->jump_in() );

            rv = outcome::success();

            return rv;
        }
        auto jump_out()
            -> kmap::Result< void >
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
            -> kmap::Result< void >
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
            KM_RESULT_PROLOG();

            auto const jstack = KTRYE( km_.fetch_component< com::JumpStack >() );

            jstack->threshold( max );
        }
        auto stack() const
            -> std::vector< Uuid >
        {
            KM_RESULT_PROLOG();

            auto const jstack = KTRYE( km_.fetch_component< com::JumpStack >() );

            return jstack->stack()
                 | rvs::transform( []( auto const& e ){ return e.id; } )
                 | ranges::to< std::vector >();
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
            .function( "format_node_label", &kmap::com::binding::JumpStack::format_node_label )
            .function( "jump_in", &kmap::com::binding::JumpStack::jump_in )
            .function( "jump_out", &kmap::com::binding::JumpStack::jump_out )
            .function( "push_transition", &kmap::com::binding::JumpStack::push_transition )
            .function( "set_threshold", &kmap::com::binding::JumpStack::set_threshold )
            .function( "stack", &kmap::com::binding::JumpStack::stack )
            ;
    }
} // namespace binding

} // namespace kmap::com
