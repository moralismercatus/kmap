/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_TAG_HPP
#define KMAP_PATH_NODE_VIEW2_TAG_HPP

#include "common.hpp"
#include "path/view/link.hpp"

#include <optional>
#include <memory>

namespace kmap::view2 {

// TODO: Awaiting compounding predicate ability, so we can do something like: tag( resolve( active_tagn ) );
auto const tag = view2::attr | view2::child( "tag" );

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_TAG_HPP
