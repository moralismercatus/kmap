/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/act/fetch_body.hpp"

#include "com/network/network.hpp"
#include "error/network.hpp"
#include "kmap.hpp"

namespace kmap::view::act {

auto fetch_body( Kmap const& km )
    -> FetchBody
{
    return FetchBody{ km };
}

auto operator|( Intermediary const& i, FetchBody const& op )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const nw = KTRY( op.km.fetch_component< com::Network >() );
    auto const ns = i | view::to_node_set( op.km );

    KMAP_ENSURE( ns.size() == 1, error_code::network::ambiguous_path );

    rv = KTRY( nw->fetch_body( *( ns.begin() ) ) );

    return rv;
}

} // namespace kmap::view::act