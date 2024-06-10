/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_ANCESTRU_HPP
#define KMAP_PATH_ANCESTRY_HPP

#include <com/network/network.hpp>
#include <contract.hpp>
#include <error/result.hpp>
#include <kmap.hpp>
#include <util/concepts.hpp>
#include <utility.hpp>

#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

namespace kmap::path {

auto fetch_ancestry_ordered( Kmap const& km
                           , Uuid const node )
    -> Result< UuidVec >;

} // namespace kmap::path

#endif // KMAP_PATH_ANCESTRY_HPP
