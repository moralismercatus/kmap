/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "lineage.hpp"

#include "common.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "error/network.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

using namespace ranges;

namespace kmap {

Lineage::Lineage( UuidVec const& nodes
                , bool const& ascending )
    : nodes_{ nodes }
    , ascending_{ ascending }
{
}

auto Lineage::begin() const
    -> UuidVec::const_iterator
{
    return nodes_.cbegin();
}

auto Lineage::end() const
    -> UuidVec::const_iterator
{
    return nodes_.cend();
}

auto Lineage::is_ascending() const
    -> bool
{
    if( nodes_.empty() )
    {
        return true;
    }

    return ascending_;
}

auto Lineage::is_descending() const
    -> bool
{
    if( nodes_.empty() )
    {
        return true;
    }

    return !ascending_;
}

auto Lineage::reverse()
    -> Lineage&
{
    nodes_ = nodes_
           | views::reverse
           | to< UuidVec >();

    return *this;
}

auto Lineage::to_vector() const
    -> UuidVec const&
{
    return nodes_;
}

auto Lineage::make( Kmap const& kmap
                  , UuidVec const& nodes )
    -> Result< Lineage >
{
    auto rv = KMAP_MAKE_RESULT( Lineage );

    if( nodes.empty() )
    {
        rv = Lineage{};
    }
    else if( kmap::is_ascending( kmap, nodes ) )
    {
        return Lineage{ nodes, true };
    }
    else if( kmap::is_descending( kmap, nodes ) )
    {
        return Lineage{ nodes, false };
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::network::invalid_lineage );
    }

    return rv;
}

auto to_heading_path( Kmap const& kmap
                    , LinealRange const& lr )
    -> std::string
{
    auto const ts = lr
                  | views::transform( [ & ]( auto const& e ){ return kmap.fetch_heading( e ).value(); } )
                  | to< StringVec >();

    return ts
         | views::join( '.' )
         | to< std::string >();
}

Lineal::Lineal( Kmap const& kmap
              , Uuid const& root
              , Uuid const& leaf )
    : kmap_{ kmap }
    , root_{ root }
    , leaf_{ leaf }
{
}

auto Lineal::root() const
    -> Uuid const&
{
    if( !( kmap_.get().is_lineal( root_, leaf_ ) ) )
    {
        KMAP_THROW_EXCEPTION_MSG( io::format( "nodes are not lineal: {}, {}", root_, leaf_ ) );
    }

    return root_;
}

auto Lineal::leaf() const
    -> Uuid const&
{
    if( !( kmap_.get().is_lineal( root_, leaf_ ) ) )
    {
        KMAP_THROW_EXCEPTION_MSG( io::format( "nodes are not lineal: {}, {}", root_, leaf_ ) );
    }

    return leaf_;
}

auto Lineal::make( Kmap const& kmap
                 , Uuid const& root 
                 , Uuid const& leaf )
    -> Result< Lineal >
{
    auto rv = KMAP_MAKE_RESULT( Lineal );

    if( kmap.is_lineal( root, leaf ) )
    {
        rv = Lineal{ kmap, root, leaf };
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::node::not_lineal ); // TODO: payload{ ancestor, descendant }
    }

    return rv;
}

auto Descendant::make( Kmap const& kmap
                     , Uuid const& ancestor
                     , Uuid const& descendant )
    -> Result< Descendant >
{
    auto rv = KMAP_MAKE_RESULT( Descendant );

    if( kmap.is_ancestor( ancestor, descendant ) )
    {
        rv = Descendant{ kmap, ancestor, descendant };
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::node::not_descendant ); // TODO: payload{ ancestor, descendant }
    }

    return rv;
}

Descendant::Descendant( Kmap const& kmap
                      , Uuid const& ancestor
                      , Uuid const& descendant )
    : Lineal{ kmap, ancestor, descendant }
{
}

LinealRange::LinealRange( UuidVec const& lineage )
    : lineage_{ lineage }
{
}

auto LinealRange::make( Kmap const& kmap
                      , Lineal const& lin )
    -> Result< LinealRange >
{
    auto rv = KMAP_MAKE_RESULT( LinealRange );
    auto lineage = kmap.absolute_path_uuid( lin );

    rv = LinealRange{ lineage };

    return rv;
}

auto LinealRange::begin()
    -> UuidVec::iterator 
{
    return lineage_.begin();
}

auto LinealRange::begin() const
    -> UuidVec::const_iterator 
{
    return lineage_.begin();
}

auto LinealRange::end() const
    -> UuidVec::const_iterator
{
    return lineage_.end();
}

} // namespace kmap