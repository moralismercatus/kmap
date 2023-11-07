/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/jump_stack/jump_stack.hpp>
#include <com/visnetwork/visnetwork.hpp> // "format_node_label"
#include <path/disambiguate.hpp>

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
            auto const nl = com::format_node_label( km_, node );
            auto const disset = KTRY( disambiguate_path3( km_, node ) );

            if( disset.empty() )
            {
                rv = nl;
            }
            else
            {
                KMAP_ENSURE( disset.contains( node ), error_code::common::uncategorized );

                auto const disroot_path = disset.at( node );

                if( disroot_path.empty() )
                {
                    rv = nl;
                }
                else
                {
                    rv = fmt::format( "{}<br>{}", nl, disroot_path );
                }
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
