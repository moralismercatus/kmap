/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/anchor/abs_root.hpp"

#include "com/database/root_node.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2::anchor {

auto AbsRoot::fetch( FetchContext const& ctx ) const
    -> FetchSet
{
    auto const rn = KTRYE( ctx.km.fetch_component< com::RootNode >() );

    return FetchSet{ LinkNode{ .id = rn->root_node() } };
}

auto AbsRoot::to_string() const
    -> std::string
{
    return "abs_root";
}

auto AbsRoot::compare_less( Anchor const& other ) const
    -> bool
{
    auto const cmp = compare_anchors( *this, other );

    return cmp == std::strong_ordering::less;
}

} // namespace kmap::view2::anchor
