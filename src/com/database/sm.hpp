/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_SM_HPP
#define KMAP_DB_SM_HPP

#include "common.hpp"
// #include "db.hpp"
#include "../../common.hpp"
#include "contract.hpp"
#include "kmap.hpp"
#include "util/sm/logger.hpp"

#include <boost/sml.hpp>
#include <range/v3/action/remove_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/algorithm/contains.hpp>

#include <memory>
#include <type_traits>

namespace kmap::db {

class Cache;

namespace sm::ev
{
    struct Push
    {
        TableVariant table = {};
        UniqueKeyVariant key = {};
        ValueVariant value = {};
    };
    struct Erase
    {
        TableVariant table = {};
        UniqueKeyVariant key = {};
    };

    namespace detail
    {
        inline auto const push = boost::sml::event< sm::ev::Push >;
        inline auto const erase = boost::sml::event< sm::ev::Erase >;

        inline auto const anonymous = boost::sml::event< boost::sml::anonymous >;
        inline auto const any = boost::sml::event< boost::sml::_ >;
        inline auto const unexpected = boost::sml::unexpected_event< boost::sml::_ >;
    }
}

namespace sm::state
{
    // Exposed States.
    class CacheExists {};
    class CreateDelta {};
    class ClearDelta {};
    class EraseDelta {};
    class DeltaExists {};
    class Error {};
    class Nop {};
    class Start {};
    class UpdateDelta {};

    namespace detail
    {
        inline auto const CacheExists = boost::sml::state< sm::state::CacheExists >;
        inline auto const CreateDelta = boost::sml::state< sm::state::CreateDelta >;
        inline auto const ClearDelta = boost::sml::state< sm::state::ClearDelta >;
        inline auto const EraseDelta = boost::sml::state< sm::state::EraseDelta >;
        inline auto const DeltaExists = boost::sml::state< sm::state::DeltaExists >;
        inline auto const Error = boost::sml::state< sm::state::Error >;
        inline auto const Nop = boost::sml::state< sm::state::Nop >;
        inline auto const Start = boost::sml::state< sm::state::Start >;
        inline auto const UpdateDelta = boost::sml::state< sm::state::UpdateDelta >;
    }
}

class CacheDeciderSm
{
public:
    struct Output
    {
        bool done = false; // TODO: I'm beginning to think this is redundant. Equivalent to any( is< Nop >, is< ... > ). Why maintain this when SM does it for me?
        std::string error_msg   = "unknown error";
    };
    using OutputPtr = std::shared_ptr< Output >;

    CacheDeciderSm( Cache const& cache
                  , OutputPtr output );

    // TODO: Uncomment. Copy ctor should be deleted, or implemented s.t. pts are deep copied. It doesn't make sense that the same ptr< output > is shared between multiple instances.
    //       Boost.SML seems to complain.
    // CacheDeciderSm( CacheDeciderSm const& ) = delete;
    // CacheDeciderSm( CacheDeciderSm&& ) = default;

    // Table
    auto operator()()
    {
        using namespace boost::sml;
        using namespace kmap::db::sm::ev::detail;
        using namespace kmap::db::sm::state::detail;
        using namespace ranges;

        /* Guards */
        auto const cached_exists = [ & ]( auto const& ev )
        {
            return contains_cached( cache_, ev.table, ev.key );
        };
        auto const cached_matches = [ & ]( auto const& ev )
        {
            auto const cv = fetch_cached( cache_, ev.table, ev.key );

            return cv && ( cv.value() == ev.value ); 
        };
        auto const delta_created = [ & ]( auto const& ev )
        {
            auto rv = false;

            if( auto const dsr = fetch_deltas( cache_, ev.table, ev.key )
              ; dsr )
            {
                auto const& ds = dsr.value();
                auto const pred = []( auto const& e )
                {
                    return e.action == DeltaType::created;
                };

                rv = ranges::find_if( ds, pred ) != ds.end();
            }

            return rv;
        };
        auto const delta_deleted = [ & ]( auto const& ev )
        {
            auto rv = false;

            if( auto const dsr = fetch_deltas( cache_, ev.table, ev.key )
              ; dsr )
            {
                auto const& ds = dsr.value();
                auto const pred = []( auto const& e )
                {
                    return e.action == DeltaType::erased;
                };

                rv = ranges::find_if( ds, pred ) != ds.end();
            }

            return rv;
        };
        auto const delta_exists = [ & ]( auto const& ev )
        {
            return contains_delta( cache_, ev.table, ev.key );
        };
        auto const delta_matches = [ & ]( auto const& ev )
        {
            auto rv = false;

            if( auto const dv = fetch_deltas( cache_, ev.table, ev.key )
              ; dv )
            {
                if( auto const& ds = dv.value()
                  ; !ds.empty() )
                {
                    rv = ( ds.back().value == ev.value );
                }
            }

            return rv;
        };

        /* Actions */
        auto const set_done = [ & ]( auto const& msg )
        {
            output_->done = true;
        };
        // auto const set_error_msg = [ & ]( auto const& msg )
        // {
        //     return [ & ]( auto const& ) mutable
        //     {
        //         output_->error_msg = msg;
        //     };
        // };

        /* Helpers */
        auto const err = [ & ]( auto const& msg )
        {
            output_->error_msg = msg;

            return Error;
        };

        return make_transition_table
        (
        *   Start + push [ delta_exists ]    = DeltaExists
        ,   Start + push [ cached_exists ]   = CacheExists
        ,   Start + push                     = CreateDelta
        ,   Start + erase [ delta_exists ]   = DeltaExists
        ,   Start + erase [ cached_exists ]  = CacheExists
        ,   Start + erase                    = err( "nothing to delete" )
        ,   Start + any                      = Error  
        ,   Start + unexpected               = Error  

        ,   DeltaExists + push [ delta_deleted ]   = err( "cache item previously deleted" ) // Treating as an error, but `delete <id>, create <id>` isn't logically impossible, but unexpected. Subject to change if such behavior is desirable.
        ,   DeltaExists + push [ delta_matches ]   = Nop
        ,   DeltaExists + push                     = UpdateDelta
        ,   DeltaExists + erase [ delta_deleted ]  = err( "cache item previously deleted" ) // Treating as an error, but `delete <id>, create <id>` isn't logically impossible, but unexpected. Subject to change if such behavior is desirable.
        ,   DeltaExists + erase [ delta_created ]  = ClearDelta
        ,   DeltaExists + erase                    = EraseDelta
        ,   DeltaExists + any                      = Error  
        ,   DeltaExists + unexpected               = Error  

        ,   CacheExists + push [ cached_matches ]   = Nop
        ,   CacheExists + push                      = UpdateDelta
        ,   CacheExists + erase                     = EraseDelta
        ,   CacheExists + any                       = Error  
        ,   CacheExists + unexpected                = Error  

        ,   CreateDelta + push       / set_done = CreateDelta
        ,   CreateDelta + any                   = Error  
        ,   CreateDelta + unexpected            = Error  

        ,   UpdateDelta + push       / set_done = UpdateDelta
        ,   UpdateDelta + any                   = Error  
        ,   UpdateDelta + unexpected            = Error  

        ,   EraseDelta + erase / set_done                        = EraseDelta
        ,   EraseDelta + any                                     = Error  
        ,   EraseDelta + unexpected                              = Error  

        ,   ClearDelta + erase / set_done                        = ClearDelta
        ,   ClearDelta + any                                     = Error  
        ,   ClearDelta + unexpected                              = Error  

        ,   Nop + push       / set_done = Nop 
        ,   Nop + any                   = Error  
        ,   Nop + unexpected            = Error  

        ,   Error + any        / set_done = Error  
        ,   Error + unexpected / set_done = Error  
        );
    }

private:
    Cache const& cache_;
    OutputPtr output_;
};

#if KMAP_LOGGING_PATH || 0
    using CacheDeciderSmDriver = boost::sml::sm< CacheDeciderSm, boost::sml::logger< SmlLogger > >;
#else
    using CacheDeciderSmDriver = boost::sml::sm< CacheDeciderSm >;
#endif // KMAP_LOGGING_PATH
using CacheDeciderSmDriverPtr = std::shared_ptr< CacheDeciderSmDriver >;

struct CacheDecider
{
    std::shared_ptr< CacheDeciderSm > sm;
    CacheDeciderSmDriverPtr driver;
    CacheDeciderSm::OutputPtr output;
};

auto make_unique_cache_decider( Cache const& cache )
    -> CacheDecider;

} // namespace kmap::db

#endif // KMAP_DB_SM_HPP
