/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_SM_HPP
#define KMAP_PATH_SM_HPP

#include "common.hpp"
#include "contract.hpp"
#include "com/database/db.hpp"
#include "com/network/network.hpp"
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

#include <memory>
#include <type_traits>

namespace kmap
{

namespace sm::ev
{
    struct Bwd {};
    struct Cmplt {};
    struct Dis {};
    struct Fwd {};
    struct Heading { kmap::Heading heading; };
    struct Root {};

    namespace detail
    {
        inline auto const cmplt = boost::sml::event< sm::ev::Cmplt >;
        inline auto const any = boost::sml::event< boost::sml::_ >;
        inline auto const bwd = boost::sml::event< sm::ev::Bwd >;
        inline auto const dis = boost::sml::event< sm::ev::Dis >;
        inline auto const fwd = boost::sml::event< sm::ev::Fwd >;
        inline auto const heading = boost::sml::event< sm::ev::Heading >;
        inline auto const root = boost::sml::event< sm::ev::Root >;
        inline auto const anonymous = boost::sml::event< boost::sml::anonymous >;
        inline auto const unexpected = boost::sml::unexpected_event< boost::sml::_ >;
    }
}

namespace sm::state
{
    // Exposed States.
    class BwdNode;
    class Delim;
    class DisNode;
    class Done;
    class Error;
    class FwdNode;

    namespace detail
    {
        inline auto const BwdNode = boost::sml::state< sm::state::BwdNode >;
        inline auto const Decided = boost::sml::state< class Decided >;
        inline auto const Delim = boost::sml::state< sm::state::Delim >;
        inline auto const DisNode = boost::sml::state< sm::state::DisNode >;
        inline auto const Disam = boost::sml::state< class Disam >;
        inline auto const Done = boost::sml::state< sm::state::Done >;
        inline auto const Next = boost::sml::state< class Next >;
        inline auto const Check = boost::sml::state< class Check >;
        inline auto const Error = boost::sml::state< sm::state::Error >;
        inline auto const FwdNode = boost::sml::state< sm::state::FwdNode >;
        inline auto const Start = boost::sml::state< class Start >;
    }
}

template< typename Driver >
auto process_token( Driver& driver
                  , std::string const& token )
    -> void
{
    if( token == "." )
    {
        driver.process_event( sm::ev::Fwd{} );
    }
    else if( token == "," )
    {
        driver.process_event( sm::ev::Bwd{} );
    }
    else if( token == "'" )
    {
        driver.process_event( sm::ev::Dis{} );
    }
    else if( token == "/" )
    {
        driver.process_event( sm::ev::Root{} );
    }
    else if( is_valid_heading( token ) )
    {
        driver.process_event( sm::ev::Heading{ token } );
    }
    else
    {
        KMAP_THROW_EXCEPTION_MSG( io::format( "invalid token: {}", token ) );
    }
}

class UniquePathDeciderSm // TODO: Can't this go in path.cpp? That would save some compilation effort.
{
public:
    struct Output
    {
        UuidPath prospect       = {};
        UuidPath disambiguation = {};
        std::string error_msg   = "unknown error"; // TODO: Change to Result< void >, to propagate known path error codes + messages?
    };
    using OutputPtr = std::shared_ptr< Output >;

    UniquePathDeciderSm( Kmap const& kmap
                       , Uuid const& root
                       , Uuid const& start
                       , OutputPtr output );

    // TODO: Uncomment. Copy ctor should be deleted, or implemented s.t. pts are deep copied. It doesn't make sense that the same ptr< prospects > is shared between multiple instances.
    //       Boost.SML seems to complain.
    // UniquePathDeciderSm( UniquePathDeciderSm const& ) = delete;
    // UniquePathDeciderSm( UniquePathDeciderSm&& ) = default;

    // Table
    // Expects tokenizing done prior to event processing: Delim{'.'|','|'/',...} or Heading/Header.
    auto operator()() 
    {
        using namespace boost::sml;
        using namespace kmap::sm::ev::detail;
        using namespace kmap::sm::state::detail;
        using namespace ranges;

        /* Guards */
        auto const child_heading_exists = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const rv = nw->fetch_child( output_->prospect.back(), ev.heading ).has_value();
            return rv;
        };
        auto const current_heading_exists = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            return nw->fetch_heading( output_->prospect.back() ).value() == ev.heading;
        };
        auto const parent_exists = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );
            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            BC_ASSERT( nw->is_lineal( root_, output_->prospect.back() ) );

            auto const p = nw->fetch_parent( output_->prospect.back() );

            return p && output_->prospect.back() != root_;
        };
        auto const dis_parent_exists = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );
            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            BC_ASSERT( nw->is_lineal( root_, output_->disambiguation.back() ) );

            auto const p = nw->fetch_parent( output_->disambiguation.back() );

            return p && output_->prospect.back() != root_;
        };
        auto const dis_heading_exists = [ & ]( auto const& ev )
        {
            if( output_->disambiguation.empty() )
            {
                return true; // As per transition table, DisNode will not be entered without a parent.
            }
            else
            {
                auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
                auto const parent = nw->fetch_parent( output_->disambiguation.back() );

                return parent && nw->fetch_heading( parent.value() ).value() == ev.heading;
            }
        };

        /* Actions */
        auto const push_child_heading = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto& back = output_->prospect.back();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const child = nw->fetch_child( back, ev.heading ); BC_ASSERT( child );
            output_->prospect.emplace_back( child.value() );
        };
        auto const push_parent = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const parent = nw->fetch_parent( output_->prospect.back() ); BC_ASSERT( parent );
            output_->prospect.emplace_back( parent.value() );
        };
        auto const prime_disam = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );
            BC_ASSERT( output_->disambiguation.empty() );

            output_->disambiguation.emplace_back( output_->prospect.back() );
        };
        auto const disambiguate = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );
            BC_ASSERT( !output_->disambiguation.empty() );

            auto& disam = output_->disambiguation;
            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const parent = nw->fetch_parent( disam.back() ); BC_ASSERT( parent );
            auto const pheading = nw->fetch_heading( parent.value() ); BC_ASSERT( pheading );

            BC_ASSERT( pheading.value() == ev.heading );

            disam.emplace_back( parent.value() );
        };
        auto const reset_disambig = [ & ]( auto const& ev )
        {
            output_->disambiguation.clear();
        };
        auto const set_error_msg = [ & ]( auto const& msg )
        {
            return [ this, msg ]( auto const& ) mutable
            {
                // auto const& prospect = this->output_->prospect;
                // if( !prospect.empty() )
                // {
                //     if( this->nw->fetch_heading( prospect.back() ).has_value() )
                //     {
                //         fmt::print( "path sm error: '{}'| last heading: '{}'\n", msg, this->nw->fetch_heading( prospect.back() ).value() );
                //     }
                //     else
                //     {
                //         fmt::print( "path sm error: '{}'| last heading: '{}'\n", msg, "<heading>" );
                //     }
                // }
                // else
                // {
                //     fmt::print( "path sm error: prospect.empty\n" );
                // }
                this->output_->error_msg = msg;
            };
        };

        return make_transition_table
        (
        *   Start + bwd        = BwdNode 
        ,   Start + fwd        = FwdNode
        ,   Start + heading    = Delim
        ,   Start + any        = Error
        ,   Start + unexpected = Error 

        ,   Delim + fwd                                                           = FwdNode
        ,   Delim + bwd [ parent_exists ] / push_parent                           = BwdNode
        ,   Delim + bwd                   / set_error_msg( "path precedes root" ) = Error
        ,   Delim + dis [ parent_exists ] / prime_disam                           = DisNode 
        ,   Delim + dis                   / set_error_msg( "path precedes root" ) = Error
        ,   Delim + root                  / set_error_msg( "invalid root path" )  = Error 
        ,   Delim + any                                                           = Error
        ,   Delim + unexpected                                                    = Error

        ,   FwdNode + fwd                                                                      = FwdNode 
        ,   FwdNode + bwd     [ parent_exists ]        / push_parent                           = BwdNode 
        ,   FwdNode + bwd                              / set_error_msg( "path precedes root" ) = Error 
        ,   FwdNode + heading [ child_heading_exists ] / push_child_heading                    = Delim
        ,   FwdNode + heading                          / set_error_msg( "invalid heading" )    = Error
        ,   FwdNode + root                             / set_error_msg( "invalid root path" )  = Error 
        ,   FwdNode + any                                                                      = Error
        ,   FwdNode + unexpected                                                               = Error

        ,   BwdNode + fwd                                                                        = FwdNode 
        ,   BwdNode + bwd     [ parent_exists ]          / push_parent                           = BwdNode 
        ,   BwdNode + bwd                                / set_error_msg( "path precedes root" ) = Error 
        ,   BwdNode + heading [ current_heading_exists ]                                         = Delim
        ,   BwdNode + heading                            / set_error_msg( "invalid heading" )    = Error 
        ,   BwdNode + root                               / set_error_msg( "invalid root path" )  = Error 
        ,   BwdNode + any                                                                        = Error
        ,   BwdNode + unexpected                                                                 = Error

        ,   DisNode + heading [ dis_heading_exists ]   / disambiguate                          = DisNode
        ,   DisNode + heading                          / set_error_msg( "invalid heading" )    = Error
        ,   DisNode + fwd                              / reset_disambig                        = FwdNode
        ,   DisNode + bwd     [ dis_parent_exists ]    / ( reset_disambig, push_parent )       = BwdNode
        ,   DisNode + bwd                              / set_error_msg( "path precedes root" ) = Error
        ,   DisNode + dis     [ dis_parent_exists ]                                            = DisNode 
        ,   DisNode + dis                              / set_error_msg( "path precedes root" ) = Error
        ,   DisNode + any                                                                      = Error
        ,   DisNode + unexpected                                                               = Error

        ,   Error + any        = Error  
        ,   Error + unexpected = Error  
        );
    }

private:
    Kmap const& kmap_;
    Uuid const root_;
    OutputPtr output_;
};

#if KMAP_LOGGING_PATH || 0
    using UniquePathDeciderSmDriver = boost::sml::sm< UniquePathDeciderSm, boost::sml::logger< SmlLogger > >;
#else
    using UniquePathDeciderSmDriver = boost::sml::sm< UniquePathDeciderSm >;
#endif // KMAP_LOGGING_PATH
using UniquePathDeciderSmDriverPtr = std::shared_ptr< UniquePathDeciderSmDriver >;

auto make_unique_path_decider( Kmap const& kmap
                             , Uuid const& root
                             , Uuid const& start )
    -> std::pair< UniquePathDeciderSmDriverPtr, UniquePathDeciderSm::OutputPtr >;

// TODO: Introduce class-wide constraint that it cannot be in a "Complete" state with prospects.empty().
class PathDeciderSm
{
public:
    using UniqueDeciderPair = std::pair< UniquePathDeciderSmDriverPtr, UniquePathDeciderSm::OutputPtr >;
    struct Output
    {
        std::vector< UniqueDeciderPair > prospects = {};
        std::string error_msg = "unknown error";
    };
    using OutputPtr = std::shared_ptr< Output >;
    PathDeciderSm( Kmap const& kmap
                 , Uuid const& root
                 , Uuid const& selected_node
                 , OutputPtr output );

    // TODO: Uncomment. Copy ctor should be deleted, or implemented s.t. pts are deep copied. It doesn't make sense that the same ptr< prospects > is shared between multiple instances.
    //       Boost.SML seems to complain.
    // PathDeciderSm( PathDeciderSm const& ) = delete;
    // PathDeciderSm( PathDeciderSm&& ) = default;

    // Table
    // Expects tokenizing done prior to event processing: Delim{'.'|','|'*',...} or Heading/Header.
    auto operator()() 
    {
        using namespace boost::sml;
        using namespace kmap::sm::ev::detail;
        using namespace kmap::sm::state::detail;
        using namespace ranges;

        /* Guards */
        auto const is_anonymous = [ & ]( auto const& ev ) -> bool
        {
            return std::is_same_v< boost::sml::anonymous, std::remove_cvref_t< decltype( ev ) > >;
        };
        auto const is_selected_root = [ & ]( auto const& ev ) -> bool
        {
            return selected_node_ == root_;
        };
        auto const heading_exists = [ & ]( auto const& ev ) -> bool
        {
            BC_ASSERT( output_ );

            auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

            return db->contains< db::HeadingTable >( ev.heading );
        };
        auto const has_prospect = [ & ]( auto const& ev ) -> bool
        {
            BC_ASSERT( output_ );

            for( auto&& [ driver, output ] : output_->prospects )
            {
                if( !driver->is( boost::sml::state< sm::state::Error > ) )
                {
                    return true;
                }
            }

            return false;
        };

        /* Actions */
        auto const set_error_msg = [ & ]( auto const& msg )
        {
            return [ & ]( auto const& ) mutable -> void
            {
                output_->error_msg = msg;
            };
        };
        auto const throw_error = [ & ]( auto const& ) -> void
        {
            KMAP_THROW_EXCEPTION_MSG( "transition from error to error" );
        };
        auto const start_selected = [ & ]( auto const& ev ) -> void
        {
            output_->prospects.emplace_back( make_unique_path_decider( kmap_, root_, selected_node_ ) );
            output_->prospects.back().first->process_event( sm::ev::Fwd{} );
        };
        auto const start_selected_parent = [ & ]( auto const& ev ) -> void
        {
            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const parent = nw->fetch_parent( selected_node_ ); BC_ASSERT( parent );
            output_->prospects.emplace_back( make_unique_path_decider( kmap_, root_, parent.value() ) );
            output_->prospects.back().first->process_event( sm::ev::Bwd{} );
        };
        auto const start_any_leads = [ & ]( auto const& ev ) -> void
        {
            BC_ASSERT( output_ );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const filter_heading = views::filter( [ & ]( auto const& e )
            {
                auto const heading = nw->fetch_heading( e ); BC_ASSERT( heading );
                return ev.heading == heading.value();
            } );
            auto const filter_lineal = views::filter( [ & ]( auto const& e )
            {
                return nw->is_lineal( root_, e );
            } );
            auto const& nodes = nw->fetch_nodes( ev.heading );

            for( auto const& node : nodes
                                  | filter_heading // TODO: Isn't filtering the heading redundant with the call to fetch_heading( <heading> )?
                                  | filter_lineal )
            {
                output_->prospects.emplace_back( make_unique_path_decider( kmap_, root_, node ) );
                output_->prospects.back().first->process_event( sm::ev::Heading{ ev.heading } );
            }
        };
        auto const start_root = [ & ]( auto const& ev ) -> void
        {
            output_->prospects.emplace_back( make_unique_path_decider( kmap_, root_, root_ ) );
            output_->prospects.back().first->process_event( sm::ev::Fwd{} );
        };
        auto const propagate = [ & ]( auto const& ev ) -> void
        {
            for( auto&& [ driver, output ] : output_->prospects )
            {
                driver->process_event( ev );
            }
        };
        #if 0
        auto const launder = [ & ]( auto const& ev ) -> void
        {
            // Note: Strange behavior exhibited when including this code...
            //       Compiler, library, or user error. In any case, it's probably memory corruption.
            //       For now, leaving this code out. End user code must check all prospects for error state before use,
            //       as this isn't cleaning them up, because of faulty behavior described.
            BC_ASSERT( output_ );

            auto const is_in_error = []( auto&& prospect )
            {
                // return false;
                return prospect.first->is( state< sm::state::Error > );
            };
            for( auto&& prospect : output_->prospects )
            {
                if( prospect.first->is( state< sm::state::Error > ) )
                {
                    io::print( "PROSPECT IN ERROR STATE: {}\n", kmap_.absolute_path_flat( prospect.second->prospect.back() ) );
                }
            }

            output_->prospects.erase( std::remove_if( output_->prospects.begin()
                                                    , output_->prospects.end()
                                                    , is_in_error ) );
            // ranges::remove_if( output_->prospects, is_in_error ); // Causes system fault.
        };
        #endif // 0

        return make_transition_table
        (
        *   Start + fwd                          / start_selected                        = Next
        ,   Start + bwd     [ is_selected_root ] / set_error_msg( "path precedes root" ) = Error 
        ,   Start + bwd                          / start_selected_parent                 = Next
        ,   Start + heading [ heading_exists ]   / start_any_leads                       = Next
        ,   Start + heading                      / set_error_msg( "invalid heading" )    = Error
        ,   Start + root                         / start_root                            = Next
        ,   Start + dis                          / set_error_msg( "invalid heading" )    = Error
        ,   Start + any [ !is_anonymous ]                                                = Error
        ,   Start + unexpected_event< boost::sml::_ >                                    = Error 

        ,   Next + any [ !is_anonymous ] / ( /*launder,*/ propagate ) = Check
        ,   Next + unexpected_event< boost::sml::_ > = Error
        
        ,   Check [ !has_prospect ] = Error
        ,   Check                   = Next

        ,   Error + any [ !is_anonymous ] / throw_error = Error  
        ,   Error + unexpected            / throw_error = Error  
        );
    }

private:
    Kmap const& kmap_;
    Uuid const root_;
    Uuid const selected_node_;
    OutputPtr output_;
};

#if KMAP_LOGGING_PATH || 0
    using PathDeciderSmDriver = boost::sml::sm< PathDeciderSm, boost::sml::logger< SmlLogger > >;
#else
    using PathDeciderSmDriver = boost::sml::sm< PathDeciderSm >;
#endif // KMAP_LOGGING_PATH
using PathDeciderSmDriverPtr = std::shared_ptr< PathDeciderSmDriver >;

auto make_path_decider( Kmap const& kmap
                      , Uuid const& root
                      , Uuid const& selected )
    -> std::pair< PathDeciderSmDriverPtr, PathDeciderSm::OutputPtr >;

class PathCompleterSm
{
public:
    PathCompleterSm( Kmap const& kmap
                   , Uuid const& root
                   , Uuid const& selected
                   , std::string const& path );

    auto completed()
        -> std::map< std::string, Uuid >
    {
        return {};
        // return *completed_;
        // auto rv = std::map< std::string, Uuid >{};
        // auto const cmplted = completed_.first.prospects();

        // return rv;
    }

    auto operator()() 
    {
        using namespace boost::sml;
        using namespace kmap::sm::ev::detail;
        using namespace kmap::sm::state::detail;
        using namespace ranges;

        /* Guards */
        // auto const is_decider_in_error = [ & ]( auto const& ev ) -> bool
        // {
        //     return decider_.second.is( state< sm::state::Error > );
        // };
        // auto const has_propsects = [ & ]( auto const& ev ) -> bool
        // {
        //     return !decider_.first->prospects().empty();
        // };
        // auto const has_ambiguous = [ & ]( auto const& ev ) -> bool
        // {
        //     auto const paths = ( *completed_ ) | views::transform( &CompletionNode::path ) | to< std::set< std::string > >();

        //     return completed_->size() == paths.size();
        // };
        // auto const has_disambig_paths = [ & ]( auto const& ev ) -> bool
        // {
        //     return decider_.first->disambiguation_paths().size() > 0;
        // };
        // auto const is_decider_in_incomplete = [ & ]( auto const& ev ) -> bool
        // {
        //     return decider_.second.is( state< sm::state::Incomplete > );
        // };
        // auto const ends_with_delim = [ & ]( auto const& ev ) -> bool
        // {
        //     return std::set< std::string >{ ".", ",", "'" }.contains( tokens_.back() );
        // };
        // auto const ends_with_bwd = [ & ]( auto const& ev ) -> bool
        // {
        //     return tokens_.back() == ",";
        // };
        // auto const ends_with_fwd = [ & ]( auto const& ev ) -> bool
        // {
        //     return tokens_.back() == ".";
        // };
        // auto const ends_with_dis = [ & ]( auto const& ev ) -> bool
        // {
        //     return tokens_.back() == "\'";
        // };
        // auto const ends_with_root = [ & ]( auto const& ev ) -> bool
        // {
        //     return tokens_.back() == "/";
        // };
        // auto const ends_with_heading = [ & ]( auto const& ev ) -> bool
        // {
        //     return is_valid_heading( tokens_.back() );
        // };
        // /* Actions */
        // auto const complete_delayed_prospects = [ & ]( auto const& env ) -> void
        // {
        //     // auto const disambig_map = decider.first.disambiguation_paths();
        //     auto const dps = decider_.first.delayed_prospects();
        //     auto const ps = decider_.first.prospects()
        //                    | views::transform( [](auto const& e ){ return e.back(); } )
        //                    | to< std::set< Uuid > >();
        //     for( auto const& dp : dps )
        //     {
        //         BC_ASSERT( !dp.empty() );

        //         for( auto const child : nw->fetch_children( dp.back() ) )
        //         {
        //             auto const cheading = nw->fetch_heading( child ).value();

        //             if( cheading.starts_with( tokens_.back() ) )
        //             {
        //                 // TODO: What if we're in the process of disambiguating? I.e., `x.y'a'b?
        //                 //       We can't just drop_last( 1 ).
        //                 //       We can know some things, though. If any of the paths have disambig paths, then they all should.
        //                 //       If that's the case, we know we're in the process of disambiguating.
        //                 // completed_->emplace( fmt::format( "{}{}"
        //                 //                                 , tokens_ | views::drop_last( 1 ) | views::join | to< std::string >()
        //                 //                                 , cheading )
        //                 //                    , child
        //                 //                    , {} );
        //             }
        //         }
        //     }
        // };
        // auto const disambiguate = [ & ]( auto const& env ) -> void
        // {
        //     // Transform into { count=duplicate#, compnode }, filter(count>1) = ambiguous
        //     auto const ambiguous = *completed_
        //                          | views::filter( []( auto const& e ){ return e; } )
        //                          | to< std::vector< Uuid > >();
        //     for( auto const& comps = *completed_
        //        ; auto const& comp : comps )
        //     {
        //         // TODO: do disambig...
        //     }
        // };

        // while( !( sm.is( state< Done > ) || sm.is( state< Error > ) ) ) sm.process( event::cmplt )
        // Done: `complete_delayed_prospects`: Should we filter the decided out? Probably. ,. completions will let the user know they're on a decided.
        // TODO: Take case: `/meta` and `/metamorph` where completion required at `/meta`. Results will be `/meta.`, `/meta,` and `/metamorph`. That's acceptable, no?
        // TODO: `complete_delayed_prospects`: What about the case where the "prospect" is followed by a chain of dis delims? I can't simply complete the children of those.
        /**
         * S -> normal
         * S -> disambiguating
         * 
         * normal -> complete_delayed
         * normal -> 
         * 
         * disambiguating -> 
         * 
         * not_disambiguating -> delayed
         * not_disambiguating -> decided
         * 
         * delayed -> decided
         * 
         **/
        return make_transition_table
        (
        // (  * Start ) [ token_ends_heading ] / process_to_penult = Next
        // ,    Start                          / process_all = Next

        // ,    FwdNode / 
        // * Start + cmplt [ is_decider_in_error ] = Error // Ill formed path. Nothing to be done but to propagate error.
        // , Start + cmplt [ has_disambig_paths ] / complete_delayed_prospects = Disam
        // , Start + cmplt                         / complete_delayed_prospects = Disam

        // // , Disambig = Done
        // // , Normal = Done

        // , Disam + cmplt [ has_ambiguous ]  / disambiguate = Disam
        // , Disam + cmplt [ !has_ambiguous ] = Decided
        // , Disam + cmplt                                     = Done
        // , Start + cmplt [ is_decider_in_incomplete ] = Undecided
        // , Start + cmplt [  ]                             = Decided

        // , Undecided + cmplt = Disasm

        // , Decided + cmplt [ ends_with_bwd ] / complete_bwd = Done
        // , Decided + cmplt [ ends_with_fwd ] / complete_fwd = Done
        // , Decided + cmplt [ ends_with_dis ] / complete_dis = Done
        // , Decided + cmplt [ ends_with_root ] / complete_from_root = Done
        // , Decided + cmplt [ ends_with_heading ] / complete_decided_heading = Done

        // , Done + cmplt = Done
        // , Done + any = Error
        // , Done + unexpected_event< boost::sml::_ > = Error  

        // , Error + any                               = Error  
        // , Error + unexpected_event< boost::sml::_ > = Error  
        );
    }

protected:

private:
    Kmap const& kmap_;
    Uuid const root_;
    Uuid const selected_;
    std::string const path_;
    StringVec const tokens_;
    std::pair< PathDeciderSm, PathDeciderSmDriver > decider_;
    std::shared_ptr< std::set< CompletionNode > > completed_ = std::make_shared< std::set< CompletionNode > >();
};

auto walk( Kmap const& kmap
         , Uuid const& root
         , Uuid const& selected
         , StringVec const& tokens )
    -> std::pair< PathDeciderSmDriverPtr, PathDeciderSm::OutputPtr >;

} // namespace kmap

#endif // KMAP_PATH_SM_HPP
