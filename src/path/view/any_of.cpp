/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/any_of.hpp"

#include "common.hpp"
#include "path/view/act/to_string.hpp"
#include "path/view/anchor/node.hpp"
#include "path/view/child.hpp"
#include "path/view/common.hpp"
#include "test/util.hpp"
#include "util/result.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2 {

auto AnyOf::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< UuidSet >();

    rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "AnyOf::create is probably not what you want" );

    return rv;
}

auto AnyOf::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> FetchSet
{
    auto rset = FetchSet{};
    fmt::print( "[any_of] fetch\n");

    for( auto const& ls = links()
       ; auto const& link : ls )
    {
        fmt::print( "[any_of] fetching: {}\n", *link | act::to_string );
        auto const fetched = anchor::node( node ) | link | act::to_fetch_set( ctx );

        rset.insert( fetched.begin(), fetched.end() );
    }

    return rset;
}

auto AnyOf::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< AnyOf >();
}

auto AnyOf::to_string() const
    -> std::string
{
    if( auto const& ls = links()
      ; !ls.empty() )
    {
        return "any_of( 'TODO' )";
    }
    else
    {
        return "any_of";
    }
}

auto AnyOf::compare_less( Link const& other ) const
    -> bool
{
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return links() < dynamic_cast< decltype( *this )& >( other ).links();
    }
    else if( cmp == std::strong_ordering::less )
    {
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace kmap::view2
