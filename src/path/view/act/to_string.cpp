/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/to_string.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/anchor/anchor.hpp"
#include "path/view/chain.hpp"
#include "path/view/tether.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

#include <string>
#include <vector>

namespace rvs = ranges::views;

namespace kmap::view2::act {

auto operator|( Anchor const& lhs
              , ToString const& rhs )
    -> std::string
{
    return lhs.to_string();
}

auto operator|( Link const& lhs
              , ToString const& rhs )
    -> std::string
{
    auto rv = std::string{};
    auto ss = std::vector< std::string >{ lhs.to_string() };
    auto prev = lhs.prev();

    while( prev )
    {
        ss.emplace_back( prev->to_string() );

        prev = prev->prev();
    }
    
    return ss
         | rvs::reverse
         | rvs::join( '|' )
         | ranges::to< std::string >();
}

auto operator|( Tether const& lhs
              , ToString const& rhs )
    -> std::string
{
    if( lhs.tail_link )
    {
        return fmt::format( "{}|{}"
                        , lhs.anchor->to_string()
                        , ( *lhs.tail_link ) | to_string );
    }
    else
    {
        return fmt::format( "{}"
                          , lhs.anchor->to_string() );
    }
}

} // namespace kmap::view2::act
