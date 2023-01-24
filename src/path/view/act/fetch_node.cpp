/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/fetch_node.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/to_node_set.hpp"
#include "path/view/act/to_string.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2::act {

FetchNode::FetchNode( Kmap const& k )
    : km{ k }
{
}

auto fetch_node( Kmap const& km )
    -> FetchNode 
{
    return FetchNode{ km };
}

auto operator|( Tether const& lhs
              , FetchNode const& rhs )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const ns = lhs | act::to_node_set( rhs.km );

    if( ns.size() == 1 )
    {
        rv = *ns.begin();
    }
    else if( ns.size() > 1 )
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::network::ambiguous_path, lhs | act::to_string );
    }
    else // size == 0
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::network::invalid_node, lhs | act::to_string );
    }

    return rv;
}

} // namespace kmap::view2::act
