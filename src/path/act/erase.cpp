/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/act/erase.hpp"

#include "com/network/network.hpp"
#include "kmap.hpp"
#include "util/result.hpp"

namespace kmap::view::act {

auto erase( Kmap& kmap )
    -> Erase
{
    return Erase{ kmap };
}

// TODO: If one of these fails to update, left in an incomplete state (part updated, part un-updated). Needs to undo.
auto operator|( Intermediary const& i, Erase const& op )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( op.km.fetch_component< com::Network >() );

    for( auto const ns = i | view::to_node_set( op.km )
       ; auto const n : ns )
    {
        KTRY( nw->erase_node( n ) );
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap::view::act