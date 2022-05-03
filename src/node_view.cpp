/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "node_view.hpp"

#include "common.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "lineage.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <vector>

namespace kmap
{

NodeView::NodeView( Kmap const& kmap
                  , Lineal const& root_selected )
    : kmap_{ kmap }
    , root_selected_{ root_selected }
{
}

NodeView::NodeView( Kmap const& kmap
                  , Uuid const& root )
    : kmap_{ kmap }
    , root_selected_{ kmap.is_lineal( root, kmap.selected_node() ) ? make< Lineal >( kmap, root, kmap.selected_node() ).value() : make< Lineal >( kmap, root, root ).value() }
{
}

NodeView::NodeView( Kmap const& kmap )
    : NodeView{ kmap
              , kmap.root_node_id() }
{
}

auto NodeView::operator[]( Heading const& heading ) const
    -> Uuid
{
    auto const desc = kmap_.fetch_descendant( root_selected_.root()
                                            , root_selected_.leaf()
                                            , heading );

    BC_ASSERT( desc ); // TODO: This should be a throw exception.

    return desc.value();
}

} // namespace kmap
