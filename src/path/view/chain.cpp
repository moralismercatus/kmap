/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "chain.hpp"

#include "path/view/act/to_string.hpp"

#include <range/v3/algorithm/lexicographical_compare.hpp>

namespace kmap::view2 {

auto Chain::operator<( Chain const& other ) const
    -> bool
{
    auto const cmp_fn = []( auto const& lhs, auto const& rhs )
    {
        return *lhs < *rhs;
    };

    return ranges::lexicographical_compare( links, other.links, cmp_fn );
}

auto operator<<( std::ostream& os
               , Chain const& chain )
    -> std::ostream&
{
    os << ( chain | act::to_string );

    return os;
}

} // namespace kmap::view2
