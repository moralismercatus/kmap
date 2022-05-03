/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "attribute.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "error/network.hpp"
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

auto is_in_attr_tree( Kmap const& kmap
                    , Uuid const& node )
    -> bool
{
    return view::root( node )
         | view::ancestor( "$" ) // TODO: Can't I do something like view::ancestor( view::attr ); composable?
         | view::exists( kmap );
}

auto is_in_order( Kmap const& kmap
                , Uuid const& parent
                , Uuid const& child )
    -> bool
{
    auto rv = false;

    if( auto const ordern = view::root( parent )
                          | view::attr
                          | view::child( "order" )
                          | view::fetch_node( kmap ) )
    {
        if( auto const ob = kmap.fetch_body( ordern.value() )
          ; ob )
        {
            auto const split = ob.value()
                             | views::split( '\n' )
                             | to< std::vector< std::string > >();
            rv = ( 0 != ranges::count( split, to_string( child ) ) );
        }
    }

    return rv;
}

auto push_genesis( Kmap& kmap
                 , Uuid const& node )
    -> Result< Uuid >
{
	auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_in_attr_tree( kmap, rv.value() ) );
            }
        })
    ;

    auto const genesisn = KMAP_TRY( view::root( node )
                                  | view::attr
                                  | view::child( "genesis" )
                                  | view::create_node( kmap )
                                  | view::to_single );

    KMAP_TRY( kmap.update_body( genesisn, std::to_string( present_time() ) ) );

    rv = genesisn;

    return rv;
}

auto push_order( Kmap& kmap
               , Uuid const& parent
               , Uuid const& child )
    -> Result< Uuid >
{
	auto rv = KMAP_MAKE_RESULT( Uuid );

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

    auto const ordern = KMAP_TRY( view::root( parent )
                                | view::attr
                                | view::child( "order" )
                                | view::fetch_or_create_node( kmap ) );

    if( auto const b = kmap.fetch_body( ordern )
      ; b && !b.value().empty() )
    {
        auto const ub = fmt::format( "{}\n{}", b.value(), to_string( child ) );

        KMAP_TRY( kmap.update_body( ordern, ub ) );

        auto const test_body = KMAP_TRY( kmap.fetch_body( ordern ) );
    }
    else
    {
        auto const ub = fmt::format( "{}", to_string( child ) );

        KMAP_TRY( kmap.update_body( ordern, ub ) );
    }

    rv = ordern;

    return rv;
}

auto pop_order( Kmap& kmap
              , Uuid const& parent
              , Uuid const& child )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

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

    auto const pordern = KMAP_TRY( view::root( parent )
                                 | view::attr
                                 | view::child( "order" )
                                 | view::fetch_node( kmap ) );
    auto const ob = KMAP_TRY( kmap.fetch_body( pordern ) );
    auto const split = ob
                     | views::split( '\n' )
                     | to< std::vector< std::string > >();
    auto const filtered = split 
                        | views::remove( to_string( child ) )
                        | to< std::vector >();
    auto const nb = filtered
                  | views::join( '\n' )
                  | to< std::string >();

    KMAP_TRY( kmap.update_body( pordern, nb ) );

    // TODO: If no children are found, should the "order" node be erased entirely?

    rv = pordern;

    return rv;
}

} // namespace kmap::attr