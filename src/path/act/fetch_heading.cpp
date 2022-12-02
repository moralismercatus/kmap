/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/act/fetch_heading.hpp"

#include "com/network/network.hpp"
#include "error/network.hpp"
#include "kmap.hpp"

namespace kmap::view::act {

auto fetch_heading( Kmap const& km )
    -> FetchHeading
{
    return FetchHeading{ km };
}

auto operator|( Intermediary const& i, FetchHeading const& op )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();
    auto const nw = KTRY( op.km.fetch_component< com::Network >() );
    auto const ns = i | view::to_node_set( op.km );

    KMAP_ENSURE( ns.size() == 1, error_code::network::ambiguous_path );

    rv = KTRY( nw->fetch_heading( *( ns.begin() ) ) );

    return rv;
}

} // namespace kmap::view::act