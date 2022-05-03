/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "sm.hpp"

#include "../common.hpp"
#include "../contract.hpp"

using namespace ranges;

namespace kmap::db {

CacheDeciderSm::CacheDeciderSm( Cache const& cache
                              , OutputPtr output )
    : cache_{ cache } 
    , output_{ output }
{
    KMAP_ENSURE_EXCEPT( output_ );
}

auto make_unique_cache_decider( Cache const& cache )
    -> std::pair< CacheDeciderSmDriverPtr, CacheDeciderSm::OutputPtr >
{
    auto output = std::make_shared< CacheDeciderSm::Output >();
    auto sm = CacheDeciderSm{ cache, output };
    auto driver = [ & ]
    {
#if KMAP_LOGGING_PATH || 0
        auto sm_logger = SmlLogger{};
        return std::make_shared< CacheDeciderSmDriver >( sm, sm_logger );
#else
        return std::make_shared< CacheDeciderSmDriver >( sm );
#endif // KMAP_LOGGING_PATH
    }();

    return { driver, output };
}

// auto walk( Kmap const& kmap
//          , Uuid const& root
//          , Uuid const& selected
//          , StringVec const& tokens )
//     -> std::pair< PathDeciderSmDriverPtr, PathDeciderSm::OutputPtr >
// {
//     auto [ driver, output ] = make_path_decider( kmap, root, selected );

//     for( auto const& token : tokens )
//     {
//         if( driver->is( boost::sml::state< sm::state::Error > ) )
//         {
//             break;
//         }

//         process_token( *driver, token );
//     }

//     return { driver, output };
// }

} // namespace kmap::db
