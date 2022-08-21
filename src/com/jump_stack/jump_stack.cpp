/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "jump_stack.hpp"

#include "emcc_bindings.hpp"
#include "kmap.hpp"

namespace kmap::com {

JumpStack::JumpStack( Kmap& km
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ km, requisites, description }
    , eclerk_{ km }
{
}

auto JumpStack::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // TODO: Calling this causes memory corruption, for some reason.
    //       The corruption only manifests later, so it's a suble bug.
    //       Perhaps address sanitizier would detect it. Anyway, disabling for now.
    // KTRY( initialize_event_outlets() );

    rv = outcome::success();

    return rv;
}

auto JumpStack::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto JumpStack::initialize_event_outlets()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.install_outlet( Leaf{ .heading = "network.select_node"
                                      , .requisites = { "subject.kmap", "verb.selected", "object.node" }
                                      , .description = "pushes non-adjacent node selections into the jump stack"
                                      , .action = R"%%%(kmap.jump_stack().jump_in( kmap.selected_node() );)%%%" } ) );

    rv = outcome::success();

    return rv;
}

auto JumpStack::jump_in( Uuid const& node )
    -> void//Optional< Uuid >
{
    KMAP_THROW_EXCEPTION_MSG( "TODO" );
}

auto JumpStack::jump_out()
    -> Optional< Uuid >
{
    KMAP_THROW_EXCEPTION_MSG( "TODO" );
}

namespace {
namespace jump_stack_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::JumpStack
,   std::set({ "root_node"s, "event_store"s })
,   "maintains non-adjacent node selection history"
);

} // namespace jump_stack_def 

namespace binding
{
    using namespace emscripten;

    struct JumpStack
    {
        Kmap& km_;

        auto jump_in( Uuid const& node )
            -> kmap::binding::Result< void >
        {
            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            jstack->jump_in( node );

            return rv;
        }
        auto jump_out()
            -> kmap::binding::Result< void >
        {
            auto rv = KMAP_MAKE_RESULT( void );
            auto const jstack = KTRY( km_.fetch_component< com::JumpStack >() );

            jstack->jump_out();

            return rv;
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
            .function( "jump_in", &kmap::com::binding::JumpStack::jump_in )
            .function( "jump_out", &kmap::com::binding::JumpStack::jump_out )
            ;
    }
} // namespace binding
} // namespace anon

} // namespace kmap::com
