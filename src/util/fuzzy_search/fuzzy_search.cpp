/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <util/fuzzy_search/fuzzy_search.hpp>

#include <com/database/db.hpp>
#include <kmap.hpp>
#include <common.hpp>

#include <fmt/format.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <zadeh.h>

#include <string>
#include <vector>

namespace rvs = ranges::views;

namespace kmap::util {

auto fuzzy_search( std::vector< std::string > const& candidates
                 , std::string const& query
                 , unsigned const& limit )
    -> std::vector< std::string >
{
    auto saf = zadeh::ArrayFilterer< std::vector< std::string >, std::string >{};

    saf.set_candidates( candidates );

    auto const results = saf.filter_indices( query
                                           , 0 // => Default; no max results.
                                           , false // Disable; no special handling of file paths.
                                           , false // Default; no special file extension handling.
                                           );

    return results
         | rvs::transform( [ &candidates ]( auto const& index ){ return candidates.at( index ); } )
         | rvs::take( limit )
         | ranges::to< std::vector >();
}

auto fuzzy_search_titles( Kmap const& km
                        , std::string const& query
                        , unsigned const& limit )
    -> std::vector< std::pair< Uuid, std::string > >
{
    KM_RESULT_PROLOG();

    auto rv = std::vector< std::pair< Uuid, std::string > >{};
    auto const db = KTRYE( km.fetch_component< com::Database >() );
    auto const& tbl = db->fetch< com::db::TitleTable >();
    auto candidates = std::vector< std::string >{};
    for( auto const& e : tbl )
    {
        candidates.emplace_back( e.right() );
    }

    // auto const candidates = tbl
    //                       | rvs::transform( [ & ]( auto const& e ){ BC_ASSERT( !e.delta_items.empty() ); return e.delta_items.back(); } )
    //                       | ranges::to< std::vector< std::string > >();
    // TODO: get titles as candidates...
    auto const results = fuzzy_search( candidates, query, limit );

    // |title|disambig|
    for( auto const& r : results )
    {
        fmt::print( "result: {}\n", r );
        auto const ids = KTRYE( tbl.fetch( r ) );

        for( auto const& id : ids )
        {
            rv.emplace_back( id.left(), id.right() );
        }
    }

    return rv;
}

} // namespace kmap::util
