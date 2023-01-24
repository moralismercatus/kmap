/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/act/select_node.hpp"

#include "com/network/network.hpp"
#include "error/network.hpp"
#include "kmap.hpp"
#include "util/result.hpp"

namespace kmap::view::act {

auto select_node( Kmap& km )
    -> SelectNode 
{
    return { km };
}

auto operator|( Intermediary const& i, SelectNode const& op )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( op.km.fetch_component< com::Network >() );
    auto const ns = i | view::to_node_set( op.km );

    KMAP_ENSURE( ns.size() == 1, error_code::network::ambiguous_path );

    KTRY( nw->select_node( *( ns.begin() ) ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::view::act