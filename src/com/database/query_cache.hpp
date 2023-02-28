/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_QUERY_CACHE_HPP
#define KMAP_DB_QUERY_CACHE_HPP

#include "common.hpp"
#include "path/node_view2.hpp"

namespace kmap::com::db {

class QueryCache
{
    using TetherMap = std::map< view2::Tether, UuidSet >;

    TetherMap map_ = {};

public:
    // auto normalize( view2::Tether const& tether )
    //     -> view2::Tether;
    auto clear()
        -> void;
    auto fetch( view2::Tether const& tether ) const
        -> Result< UuidSet >;
    auto push( view2::Tether const& tether
             , UuidSet const& result )
        -> Result< void >;

    auto begin() const
        -> TetherMap::const_iterator;
    auto end() const
        -> TetherMap::const_iterator;
};

} // kmap::com::db


#endif // KMAP_DB_QUERY_CACHE_HPP