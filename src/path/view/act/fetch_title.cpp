/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/fetch_title.hpp"

#include "com/network/network.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/fetch_node.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2::act {

FetchTitle::FetchTitle( Kmap const& k )
    : km{ k }
{
}

auto fetch_title( Kmap const& km )
    -> FetchTitle 
{
    return FetchTitle{ km };
}

auto operator|( Tether const& lhs
              , FetchTitle const& rhs )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( std::string );
    auto const n = KTRY( lhs | act::fetch_node( rhs.km ) );
    auto const nw = KTRY( rhs.km.fetch_component< com::Network >() );

    rv = KTRY( nw->fetch_title( n ) );

    return rv;
}

} // namespace kmap::view2::act
