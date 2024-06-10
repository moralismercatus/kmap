/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <cmd/command.hpp>
#include <com/cmd/command.hpp>
#include <com/network/network.hpp>
#include <kmap.hpp>
#include <utility.hpp>
#include <util/result.hpp>

#include <emscripten/bind.h>

namespace kmap::cmd {
namespace command::binding {

using namespace emscripten;

auto create_command( std::string const& path )
    -> kmap::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( Uuid );

    // auto& kmap = Singleton::instance();
    auto const prereg = com::Command{ path
                                    , "Undescribed"
                                    , {}
                                    , { "unconditional", "```javascript\nreturn kmap.success( 'unconditional' );\n```" }
                                    , "```kscript\n:echo Command Unimplemented\n```" };
    // auto const cmd = kmap.cli().create_command( prereg );

    // rv = cmd;

    return rv;
}

auto fetch_nearest_command( Uuid const& node )
    -> kmap::Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", node );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const& kmap = Singleton::instance();
    auto const cmd_root = KTRY( view2::cmd::command_root
                              | act2::fetch_node( kmap ) );

    auto parent = Optional< Uuid >( node );

    while( parent
        && ( parent.value() != cmd_root ) )
    {
        auto const p = parent.value();

        if( is_general_command( kmap, p ) )
        {
            rv = p;
        }
        else
        {
            auto const nw = KTRY( kmap.fetch_component< com::Network >() );

            parent = to_optional( nw->fetch_parent( p ) );
        }
    }

    return rv;
}

EMSCRIPTEN_BINDINGS( kmap_module )
{
    function( "create_command", &kmap::cmd::command::binding::create_command );
    function( "fetch_nearest_command", &kmap::cmd::command::binding::fetch_nearest_command );
}

} // namespace command::binding

} // namespace kmap::cmd