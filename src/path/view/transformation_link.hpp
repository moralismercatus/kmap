/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_TRANSFORMATION_LINK_HPP
#define KMAP_PATH_NODE_VIEW2_TRANSFORMATION_LINK_HPP

#include <common.hpp>
#include <path/view/concepts.hpp>
#include <path/view/link.hpp>
#include <util/concepts.hpp>

#include <path/view/act/to_string.hpp>

#include <concepts>
#include <initializer_list>
#include <memory>
#include <variant>
#include <vector>

namespace kmap::view2 {

class TransformationLink : public Link
{
public:
    virtual auto fetch( FetchContext const& ctx, FetchSet const& fs ) const -> Result< FetchSet > = 0;
};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_TRANSFORMATION_LINK_HPP
