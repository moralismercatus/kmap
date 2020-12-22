/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_NODE_FETCHER_HPP
#define KMAP_NODE_FETCHER_HPP

#include "common.hpp"

namespace kmap
{

class Kmap;

class NodeFetcher
{
public:
    NodeFetcher( Kmap const& kmap );

    auto operator[]( Heading const& heading ) const
        -> Uuid; 

private:
    Kmap const& kmap_;
};

} // namespace kmap

#endif // KMAP_NODE_FETCHER_HPP
