/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/derivation_link.hpp>

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2
{

auto fetch( Link const* link
          , FetchContext const& ctx
          , Uuid const& node )
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );

    auto rv = result::make_result< FetchSet >();

    DerivationLink const& dlink = KTRY( result::dyn_cast< DerivationLink const >( link ) );

    rv = KTRY( dlink.fetch( ctx, node ) );

    return rv;
}

auto fetch( PolymorphicValue< Link > const& plink
          , FetchContext const& ctx
          , Uuid const& node )
    -> Result< FetchSet >
{
    return fetch( plink.get(), ctx, node );
}

} // namespace kmap::view2
