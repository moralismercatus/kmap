/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "to_string.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

namespace kmap::view::act {

auto operator|( Intermediary const& i, ToString const& op )
    -> std::string
{
    return i.op_chain
         | ranges::views::transform( []( auto const& e ){ return std::visit( []( auto const& arg ){ return arg.to_string(); }, e ); } )
         | ranges::views::join( '|' )
         | ranges::to< std::string >();
}

} // namespace kmap::view::act