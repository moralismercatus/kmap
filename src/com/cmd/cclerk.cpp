/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/cmd/cclerk.hpp"

#include "com/cmd/command.hpp"
#include "kmap.hpp"

#include <range/v3/view/reverse.hpp>

namespace kmap::com {

CommandClerk::CommandClerk( Kmap& km )
    : kmap{ km }
{
}

CommandClerk::~CommandClerk()
{
    if( auto cstorer = kmap.fetch_component< CommandStore >() // TODO: What to do if fails?
      ; cstorer ) 
    {
        auto& cstore = cstorer.value();
        auto const handle_result = []( auto const& res )
        {
            if( !res
             && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
            {
                KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
            }
        };

        for( auto const& e : commands | ranges::views::reverse ) { handle_result( cstore->uninstall_command( e ) ); }
        for( auto const& e : arguments | ranges::views::reverse ) { handle_result( cstore->uninstall_argument( e ) ); }
    }
}

auto CommandClerk::install_argument( Argument const& arg )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto cstore = KTRY( kmap.fetch_component< CommandStore >() );

    auto const argn = KTRY( cstore->install_argument( arg ) );

    arguments.emplace_back( argn );

    rv = argn;

    return rv;
}

auto CommandClerk::install_command( Command const& cmd )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto cstore = KTRY( kmap.fetch_component< CommandStore >() );

    auto const cmdn = KTRY( cstore->install_command( cmd ) );

    commands.emplace_back( cmdn );

    rv = cmdn;

    return rv;
}

} // namespace kmap::com
