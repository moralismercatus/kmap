/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "node_fetcher.hpp"

#include "contract.hpp"
#include "kmap.hpp"

namespace kmap
{

NodeFetcher::NodeFetcher( Kmap const& kmap )
    : kmap_{ kmap }
{
}

auto NodeFetcher::operator[]( Heading const& heading ) const
    -> Uuid
{
    auto const leaf = kmap_.fetch_leaf( heading );

    BC_ASSERT( leaf ); // TODO: This should be a throw exception.

    return *leaf;
}

} // namespace kmap
