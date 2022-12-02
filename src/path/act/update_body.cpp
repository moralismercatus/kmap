/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "update_body.hpp"

#include "com/network/network.hpp"
#include "kmap.hpp"

namespace kmap::view::act {

UpdateBody::UpdateBody( Kmap& kmap
                      , std::string const& body )
    : km{ kmap }
    , content{ body }
{
}

auto update_body( Kmap& kmap
                , std::string const& body )
    -> UpdateBody
{
    return UpdateBody{ kmap, body };
}

// TODO: If one of these fails to update, left in an incomplete state (part updated, part un-updated). Needs to undo.
auto operator|( Intermediary const& i, UpdateBody const& op )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const nw = KTRY( op.km.fetch_component< com::Network >() );

    for( auto const ns = i | view::to_node_set( op.km )
       ; auto const n : ns )
    {
        KTRY( nw->update_body( n, op.content ) );
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap::view::act