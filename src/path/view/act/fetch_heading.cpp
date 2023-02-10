/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/fetch_heading.hpp"

#include "com/network/network.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/fetch_node.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2::act {

FetchHeading::FetchHeading( Kmap const& k )
    : km{ k }
{
}

auto fetch_heading( Kmap const& km )
    -> FetchHeading 
{
    return FetchHeading{ km };
}

auto operator|( Tether const& lhs
              , FetchHeading const& rhs )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const n = KTRY( lhs | act::fetch_node( rhs.km ) );
    auto const nw = KTRY( rhs.km.fetch_component< com::Network >() );

    rv = KTRY( nw->fetch_heading( n ) );

    return rv;
}

} // namespace kmap::view2::act
