/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/fetch_body.hpp"

#include "com/network/network.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/fetch_node.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2::act {

FetchBody::FetchBody( Kmap const& k )
    : km{ k }
{
}

auto fetch_body( Kmap const& km )
    -> FetchBody 
{
    return FetchBody{ km };
}

auto operator|( Tether const& lhs
              , FetchBody const& rhs )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const n = KTRY( lhs | act::fetch_node( rhs.km ) );
    auto const nw = KTRY( rhs.km.fetch_component< com::Network >() );

    rv = KTRY( nw->fetch_body( n ) );

    return rv;
}

} // namespace kmap::view2::act
