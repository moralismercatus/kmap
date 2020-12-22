/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "text.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"

namespace kmap::cmd {

auto edit_body( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >
{   
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.empty() );
            })
        ;

        auto const& target = kmap.selected_node();

        kmap.load_body_editor( target );

        return { CliResultCode::success
                , "node open for editing" };
    };
}

auto view_body( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        (void)kmap;
        return { CliResultCode::failure
               , "todo" };
    };
}

} // namespace kmap::cmd