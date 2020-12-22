/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "sm.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../kmap.hpp"
#include "../path.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

using namespace ranges;

namespace kmap {

UniquePathDeciderSm::UniquePathDeciderSm( Kmap const& kmap
                                        , Uuid const& root
                                        , Uuid const& start
                                        , OutputPtr output )
    : kmap_{ kmap } 
    , root_{ root }
    , output_{ output }
{
    BC_ASSERT( output_ );

    output_->prospect = decltype( output_->prospect ){ start };
}

auto make_unique_path_decider( Kmap const& kmap
                             , Uuid const& root
                             , Uuid const& start )
    -> std::pair< UniquePathDeciderSmDriverPtr, UniquePathDeciderSm::OutputPtr >
{
    auto output = std::make_shared< UniquePathDeciderSm::Output >();
    auto sm = UniquePathDeciderSm{ kmap 
                                 , root
                                 , start
                                 , output };
    auto driver = [ & ]
    {
#if KMAP_LOGGING_PATH || 0
        auto sm_logger = SmlLogger{};
        return std::make_shared< UniquePathDeciderSmDriver >( sm, sm_logger );
#else
        return std::make_shared< UniquePathDeciderSmDriver >( sm );
#endif // KMAP_LOGGING_PATH
    }();

    return { driver, output };
}

PathDeciderSm::PathDeciderSm( Kmap const& kmap
                            , Uuid const& root
                            , Uuid const& selected_node
                            , OutputPtr output )
    : kmap_{ kmap } 
    , root_{ root }
    , selected_node_{ selected_node }
    , output_{ output }
{
}

auto make_path_decider( Kmap const& kmap
                      , Uuid const& root
                      , Uuid const& selected )
    -> std::pair< PathDeciderSmDriverPtr, PathDeciderSm::OutputPtr >
{
    auto output = std::make_shared< PathDeciderSm::Output >();
    auto sm = PathDeciderSm{ kmap 
                           , root
                           , selected
                           , output };
    auto driver = [ & ]
    {
#if KMAP_LOGGING_PATH
        auto sm_logger = SmlLogger{};
        return std::make_shared< PathDeciderSmDriver >( sm, sm_logger );
#else
        return std::make_shared< PathDeciderSmDriver >( sm );
#endif // KMAP_LOGGING_PATH
    }();

    return { driver, output };
}

auto walk( Kmap const& kmap
         , Uuid const& root
         , Uuid const& selected
         , StringVec const& tokens )
    -> std::pair< PathDeciderSmDriverPtr, PathDeciderSm::OutputPtr >
{
    auto [ driver, output ] = make_path_decider( kmap, root, selected );

    for( auto const& token : tokens )
    {
        if( driver->is( boost::sml::state< sm::state::Error > ) )
        {
            break;
        }

        process_token( *driver, token );
    }

    return { driver, output };
}

} // namespace kmap
