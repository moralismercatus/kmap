/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/to_fetch_set.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/anchor/anchor.hpp"

#include <catch2/catch_test_macros.hpp>

// testing
#include "utility.hpp"

namespace kmap::view2::act {

auto to_fetch_set( FetchContext const& ctx )
    -> ToFetchSet 
{
    return ToFetchSet{ ctx };
}

auto operator|( Tether const& lhs
              , ToFetchSet const& rhs )
    -> FetchSet
{
    auto rv = FetchSet{};
    auto ns = lhs.anchor->fetch( rhs.ctx );
    auto const links = [ & ]
    {
        auto rlinks = std::deque< Link::LinkPtr >{};
        auto link = lhs.tail_link;
        while( link )
        {
            rlinks.emplace_front( link );
            link = link->prev();
        }
        return rlinks;
    }();

    for( auto const& link : links )
    {
        auto next_ns = decltype( ns ){};

        for( auto const& node : ns )
        {
            auto const tns = link->fetch( rhs.ctx, node.id );

            next_ns.insert( tns.begin(), tns.end() );
        }

        ns = next_ns;
    }

    rv = ns;

    return rv;
}

} // namespace kmap::view2::act
