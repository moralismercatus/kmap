/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/act/to_fetch_set.hpp>

#include <com/database/db.hpp>
#include <com/database/query_cache.hpp>
#include <contract.hpp>
#include <kmap.hpp>
#include <path/view/anchor/anchor.hpp>
#include <path/view/derivation_link.hpp>
#include <path/view/transformation_link.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

// testing
#include "utility.hpp"

namespace rvs = ranges::views;

namespace kmap::view2::act {

auto to_fetch_set( FetchContext const& ctx )
    -> ToFetchSet 
{
    return ToFetchSet{ ctx };
}

auto operator|( Tether const& lhs
              , ToFetchSet const& rhs )
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();

    auto rv = FetchSet{};
    auto const db = KTRY( rhs.ctx.km.fetch_component< com::Database >() );
    auto& qcache = db->query_cache();

    if( auto const qr = qcache.fetch( lhs )
      ; qr )
    {
        rv = qr.value()
           | rvs::transform( []( auto const& e ){ return LinkNode{ .id = e }; } )
           | ranges::to< FetchSet >();
    }
    else
    {
        auto fs = lhs.anchor->fetch( rhs.ctx );
        auto const links = [ & ]
        {
            auto rlinks = std::deque< Link::LinkPtr >{};
            auto link = lhs.tail_link;
            while( link )
            {
                rlinks.emplace_front( link );
                link = link->prev();
            }
            return rlinks;
        }();

        for( auto const& link : links )
        {
            auto next_fs = decltype( fs ){};

            if( auto const tlink = result::dyn_cast< view2::TransformationLink const >( link.get() )
              ; tlink )
            {
                TransformationLink const& tlinkv = tlink.value();

                next_fs = KTRY( tlinkv.fetch( rhs.ctx, fs ) );
            }
            else 
            {
                for( auto const& node : fs )
                {
                    auto const tfs = KTRY( fetch( link, rhs.ctx, node.id ) );

                    next_fs.insert( tfs.begin(), tfs.end() );
                }
            }

            fs = next_fs;
        }

        {
            auto const ns = fs
                          | rvs::transform( []( auto const& e ){ return e.id; } )
                          | ranges::to< UuidSet >();
            KTRYE( const_cast< com::db::QueryCache& >( qcache ).push( lhs, ns ) ); // TODO: WARNING FLAGS!!! VERY TEMPORARY! const_cast a no-no!
            KMAP_ENSURE_EXCEPT( qcache.fetch( lhs ) );
        }

        rv = fs;
    }

    return rv;
}

} // namespace kmap::view2::act
