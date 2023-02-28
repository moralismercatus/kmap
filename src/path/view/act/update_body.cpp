/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/update_body.hpp"

#include "com/network/network.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/fetch_node.hpp"

#include "path.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2::act {

UpdateBody::UpdateBody( Kmap& k
                      , std::string const& content )
    : km{ k }
    , content{ content }
{
}

auto update_body( Kmap& km
                , std::string const& content )
    -> UpdateBody 
{
    return UpdateBody{ km, content };
}

auto operator|( Tether const& lhs
              , UpdateBody const& rhs )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const n = KTRY( lhs | act::fetch_node( rhs.km ) );
    auto const nw = KTRY( rhs.km.fetch_component< com::Network >() );

    KTRY( nw->update_body( n, rhs.content ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::view2::act
