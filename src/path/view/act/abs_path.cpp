/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/act/abs_path.hpp"

#include "contract.hpp"
#include "kmap.hpp"
#include "path/view/act/to_node_set.hpp"
#include "path/view/tether.hpp"

#include <catch2/catch_test_macros.hpp>

// TODO: Temporary... remove after moving abs_path_flat impl. here.
#include "path/node_view.hpp"
#include "path/act/abs_path.hpp"
#include "path.hpp"
#include "com/network/network.hpp"

namespace kmap::view2::act {

auto abs_path_flat( Kmap const& km )
    -> AbsPathFlat
{
    return AbsPathFlat{ km };
}

auto operator|( Tether const& lhs
              , AbsPathFlat const& rhs )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( std::string );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( lhs.anchor );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_valid_heading_path( rv.value() ) );
            }
        })
    ;

    auto const ctx = FetchContext{ rhs.km, lhs };
    auto const av = lhs.anchor->fetch( ctx );

    if( !lhs.tail_link )
    {
        KMAP_ENSURE( av.size() == 1, error_code::common::uncategorized );

        rv = KTRY( view::make( ( av.begin()->id ) )
                 | view::act::abs_path_flat( rhs.km ) );
    }
    else
    {
        auto const ns = lhs | act::to_node_set( rhs.km );

        KMAP_ENSURE( ns.size() == 1, error_code::common::uncategorized );
        
        {
            auto const nw = KTRYE( rhs.km.fetch_component< com::Network >() );
        }

        rv = KTRY( view::make( av.begin()->id )
                 | view::desc( *( ns.begin() ) )
                 | view::act::abs_path_flat( ctx.km ) );
    }

    return rv;
}

} // namespace kmap::view2::act
