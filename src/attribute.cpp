/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "attribute.hpp"

#include "contract.hpp"
#include "com/database/db.hpp"
#include "com/network/network.hpp"
#include "error/network.hpp"
#include "kmap.hpp"
#include "path/node_view.hpp"
#include "utility.hpp"

#include <range/v3/algorithm/count.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/replace.hpp>
#include <range/v3/view/split.hpp>

#include <string>

using namespace ranges;

namespace kmap::attr {

auto is_attr( Kmap const& kmap
            , Uuid const& node )
    -> bool
{
    auto const db = KTRYE( kmap.fetch_component< com::Database >() );

    return db->attr_exists( node );
}

/// Returns whether 'node' is a descendant of an attribute node.
auto is_in_attr_tree( Kmap const& kmap
                 , Uuid const& node )
    -> bool
{
    return is_attr( kmap, node )
        || ( view::make( node )
           | view::ancestor( "$" ) // TODO: Can't I do something like view::ancestor( view::attr ); composable?
           | view::exists( kmap ) );
}

auto is_in_order( Kmap const& kmap
                , Uuid const& parent
                , Uuid const& child )
    -> bool
{
    auto rv = false;
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const rparent = nw->resolve( parent );
    auto const rchild = nw->resolve( child );

    if( auto const ordern = view::make( rparent )
                          | view::attr
                          | view::child( "order" )
                          | view::fetch_node( kmap ) )
    {
        if( auto const ob = nw->fetch_body( ordern.value() )
          ; ob )
        {
            auto const split = ob.value()
                             | views::split( '\n' )
                             | to< std::vector< std::string > >();
            rv = ( 0 != ranges::count( split, to_string( rchild ) ) );
        }
    }

    return rv;
}

auto push_genesis( Kmap& kmap
                 , Uuid const& node )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "genesis", node );

	auto rv = result::make_result< Uuid >();
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_in_attr_tree( kmap, rv.value() ) );
            }
        })
    ;

    auto const genesisn = KMAP_TRY( view::make( node )
                                  | view::attr
                                  | view::child( "genesis" )
                                  | view::create_node( kmap )
                                  | view::to_single );

    KMAP_TRY( nw->update_body( genesisn, std::to_string( present_time() ) ) );

    rv = genesisn;

    return rv;
}

auto push_order( Kmap& kmap
               , Uuid const& parent
               , Uuid const& child )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_NODE( "child", child );

	auto rv = result::make_result< Uuid >();
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_in_order( kmap, parent, child ) );
            }
        })
    ;

    KMAP_ENSURE( !is_in_order( kmap, parent, child ), error_code::network::attribute );

    auto const rparent = nw->resolve( parent );
    auto const rchild = nw->resolve( child );

    KMAP_ENSURE( !is_in_order( kmap, rparent, rchild ), error_code::network::attribute ); // TODO: Is this redundant with the other ensure on unresolved nodes?

    auto const ordern = KMAP_TRY( view::make( rparent )
                                | view::attr
                                | view::child( "order" )
                                | view::fetch_or_create_node( kmap ) );

    if( auto const b = nw->fetch_body( ordern )
      ; b && !b.value().empty() )
    {
        auto const ub = fmt::format( "{}\n{}", b.value(), to_string( rchild ) );

        KMAP_TRY( nw->update_body( ordern, ub ) );

        auto const test_body = KMAP_TRY( nw->fetch_body( ordern ) );
    }
    else
    {
        auto const ub = fmt::format( "{}", to_string( rchild ) );

        KMAP_TRY( nw->update_body( ordern, ub ) );
    }

    rv = ordern;

    return rv;
}

// TODO: Unit test that $.order is erased when parent has no more children
auto pop_order( Kmap& kmap
              , Uuid const& parent
              , Uuid const& child )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "parent", parent );
        KM_RESULT_PUSH_NODE( "child", child );

    auto rv = result::make_result< Uuid >();
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( !is_in_order( kmap, parent, child ) );
            }
        })
    ;

    KMAP_ENSURE( is_in_order( kmap, parent, child ), error_code::network::attribute );

    auto const rparent = nw->resolve( parent );
    auto const rchild = nw->resolve( child );

    KMAP_ENSURE( is_in_order( kmap, rparent, rchild ), error_code::network::attribute ); // TODO: Is this redundant with the other ensure on unresolved nodes?

    auto const pordern = KMAP_TRY( view::make( rparent )
                                 | view::attr
                                 | view::child( "order" )
                                 | view::fetch_node( kmap ) );
    auto const ob = KMAP_TRY( nw->fetch_body( pordern ) );
    auto const split = ob
                     | views::split( '\n' )
                     | to< std::vector< std::string > >();
    auto const filtered = split 
                        | views::remove( to_string( rchild ) )
                        | to< std::vector >();
    if( filtered.empty() )
    {
        KTRY( nw->erase_node( pordern ) );
    }
    else
    {
        auto const nb = filtered
                    | views::join( '\n' )
                    | to< std::string >();

        KTRY( nw->update_body( pordern, nb ) );
    }

    rv = pordern;

    return rv;
}

} // namespace kmap::attr