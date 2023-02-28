/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_NODE_VIEW2_COMMON_HPP
#define KMAP_PATH_NODE_VIEW2_COMMON_HPP

#include <common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>

namespace bmi = boost::multi_index;

namespace kmap {
    class Kmap;
}

namespace kmap::view2 {

struct LinkNode
{
    Uuid id = {};

    std::strong_ordering operator<=>( LinkNode const& ) const = default;
};

using FetchSet = bmi::multi_index_container< LinkNode 
                                           , bmi::indexed_by< bmi::hashed_unique< bmi::tag< decltype( LinkNode::id ) >
                                                                                , bmi::member< LinkNode
                                                                                             , decltype( LinkNode::id )
                                                                                             , &LinkNode::id > > > >;

class Tether;

struct CreateContext
{
    Kmap& km;
    Tether const& tether;
};
struct FetchContext
{
    Kmap const& km;
    Tether const& tether;

    FetchContext( Kmap const& k
                , Tether const& c )
        : km{ k }
        , tether{ c }
    {
    }
    FetchContext( CreateContext const& ctx )
        : km{ ctx.km }
        , tether{ ctx.tether }
    {
    }
};

} // namespace kmap::view2

#endif // KMAP_PATH_NODE_VIEW2_COMMON_HPP
