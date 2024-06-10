/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_SM_HPP
#define KMAP_PATH_SM_HPP

#include <attribute.hpp>
#include <com/database/db.hpp>
#include <com/network/network.hpp>
#include <common.hpp>
#include <contract.hpp>
#include <kmap.hpp>
#include <util/sm/logger.hpp>

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
    struct Attr {};
    struct Bwd {};
    struct Cmplt {};
    struct Dis {};
    struct Fwd {};
    struct Heading { kmap::Heading heading; };
    struct Root {};
    struct Tag { kmap::Heading heading; };

    namespace detail
    {
        inline auto const anonymous = boost::sml::event< boost::sml::anonymous >;
        inline auto const any = boost::sml::event< boost::sml::_ >;
        inline auto const attrib = boost::sml::event< sm::ev::Attr >;
        inline auto const bwd = boost::sml::event< sm::ev::Bwd >;
        inline auto const cmplt = boost::sml::event< sm::ev::Cmplt >;
        inline auto const dis = boost::sml::event< sm::ev::Dis >;
        inline auto const fwd = boost::sml::event< sm::ev::Fwd >;
        inline auto const heading = boost::sml::event< sm::ev::Heading >;
        inline auto const root = boost::sml::event< sm::ev::Root >;
        inline auto const tag = boost::sml::event< sm::ev::Tag >;
        inline auto const unexpected = boost::sml::unexpected_event< boost::sml::_ >;
    }
}

namespace sm::state
{
    // Exposed States.
    class AttrNode;
    class BwdNode;
    class DisNode;
    class Done;
    class Error;
    class FwdNode;
    class Heading;
    class Tag;

    namespace detail
    {
        inline auto const AttrNode = boost::sml::state< sm::state::AttrNode >;
        inline auto const BwdNode = boost::sml::state< sm::state::BwdNode >;
        inline auto const Check = boost::sml::state< class Check >;
        inline auto const Decided = boost::sml::state< class Decided >;
        inline auto const DisNode = boost::sml::state< sm::state::DisNode >;
        inline auto const Disam = boost::sml::state< class Disam >;
        inline auto const Done = boost::sml::state< sm::state::Done >;
        inline auto const Error = boost::sml::state< sm::state::Error >;
        inline auto const FwdNode = boost::sml::state< sm::state::FwdNode >;
        inline auto const Heading = boost::sml::state< sm::state::Heading >;
        inline auto const Next = boost::sml::state< class Next >;
        inline auto const Start = boost::sml::state< class Start >;
        inline auto const Tag = boost::sml::state< sm::state::Tag >;
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
    else if( token.starts_with( '#' ) )
    {
        driver.process_event( sm::ev::Tag{} );
    }
    else if( token == "$" )
    {
        driver.process_event( sm::ev::Attr{} );
    }
    else if( is_valid_heading( token ) )
    {
        driver.process_event( sm::ev::Heading{ token } );
    }
    else
    {
        KMAP_THROW_EXCEPTION_MSG( io::format( "invalid token: '{}'", token ) );
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

private:
    Kmap const& kmap_;
    Uuid const root_;
    OutputPtr output_;

public:
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
        using kmap::sm::state::detail::Heading;

        KM_RESULT_PROLOG();

        /* Guards */
        auto const attr_node_exists = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const rv = nw->fetch_attr_node( output_->prospect.back() ).has_value();
            return rv;
        };
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
            return KTRYE( nw->fetch_heading( output_->prospect.back() ) ) == ev.heading;
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

                return parent && KTRYE( nw->fetch_heading( parent.value() ) ) == ev.heading;
            }
        };

        /* Actions */
        auto const push_attr_node = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const attrn = KTRYE( nw->fetch_attr_node( output_->prospect.back() ) );
            output_->prospect.emplace_back( attrn );
        };
        auto const push_child_heading = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto& back = output_->prospect.back();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const child = KTRYE( nw->fetch_child( back, ev.heading ) );
            output_->prospect.emplace_back( child );
        };
        auto const push_parent = [ & ]( auto const& ev )
        {
            BC_ASSERT( output_ );
            BC_ASSERT( !output_->prospect.empty() );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const parent = KTRYE( nw->fetch_parent( output_->prospect.back() ) );
            output_->prospect.emplace_back( parent );
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
            auto const parent = KTRYE( nw->fetch_parent( disam.back() ) );
            auto const pheading = KTRYE( nw->fetch_heading( parent ) );

            BC_ASSERT( pheading == ev.heading );

            disam.emplace_back( parent );
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
        ,   Start + heading    = Heading
        ,   Start + tag        = Tag
        ,   Start + any        = Error
        ,   Start + unexpected = Error 

        ,   Heading + fwd                                                           = FwdNode
        ,   Heading + bwd [ parent_exists ] / push_parent                           = BwdNode
        ,   Heading + bwd                   / set_error_msg( "path precedes root" ) = Error
        ,   Heading + dis [ parent_exists ] / prime_disam                           = DisNode 
        ,   Heading + dis                   / set_error_msg( "path precedes root" ) = Error
        ,   Heading + root                  / set_error_msg( "invalid root path" )  = Error 
        ,   Heading + any                                                           = Error
        ,   Heading + unexpected                                                    = Error

        // ,   Tag + heading [ child_tag_exists ] / push_child_tag                 = Heading
        ,   Tag + fwd                                                           = FwdNode
        ,   Tag + bwd [ parent_exists ] / push_parent                           = BwdNode
        ,   Tag + bwd                   / set_error_msg( "path precedes root" ) = Error
        ,   Tag + dis [ parent_exists ] / prime_disam                           = DisNode 
        ,   Tag + dis                   / set_error_msg( "path precedes root" ) = Error
        ,   Tag + root                  / set_error_msg( "invalid root path" )  = Error 
        ,   Tag + any                                                           = Error
        ,   Tag + unexpected                                                    = Error

        ,   FwdNode + fwd                                                                      = FwdNode 
        ,   FwdNode + bwd     [ parent_exists ]        / push_parent                           = BwdNode 
        ,   FwdNode + bwd                              / set_error_msg( "path precedes root" ) = Error 
        ,   FwdNode + heading [ child_heading_exists ] / push_child_heading                    = Heading
        ,   FwdNode + heading                          / set_error_msg( "invalid heading" )    = Error
        ,   FwdNode + attrib [ attr_node_exists ]      / push_attr_node                        = AttrNode
        ,   FwdNode + root                             / set_error_msg( "invalid root path" )  = Error 
        ,   FwdNode + any                                                                      = Error
        ,   FwdNode + unexpected                                                               = Error

        ,   BwdNode + fwd                                                                        = FwdNode 
        ,   BwdNode + bwd     [ parent_exists ]          / push_parent                           = BwdNode 
        ,   BwdNode + bwd                                / set_error_msg( "path precedes root" ) = Error 
        ,   BwdNode + heading [ current_heading_exists ]                                         = Heading
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

        ,   AttrNode + fwd                    = FwdNode
        ,   AttrNode + any                    = Error
        ,   AttrNode + unexpected             = Error

        ,   Error + any        = Error  
        ,   Error + unexpected = Error  
        );
    }
};

#if KMAP_LOGGING_PATH || 0
    using UniquePathDeciderSmDriver = boost::sml::sm< UniquePathDeciderSm, boost::sml::logger< SmlLogger > >;
#else
    using UniquePathDeciderSmDriver = boost::sml::sm< UniquePathDeciderSm >;
#endif // KMAP_LOGGING_PATH
using UniquePathDeciderSmDriverPtr = std::shared_ptr< UniquePathDeciderSmDriver >;

struct UniquePathDecider
{
    std::shared_ptr< UniquePathDeciderSm > sm;
    UniquePathDeciderSmDriverPtr driver;
    UniquePathDeciderSm::OutputPtr output;
};

auto make_unique_path_decider( Kmap const& kmap
                             , Uuid const& root
                             , Uuid const& start )
    -> UniquePathDecider;

// TODO: Introduce class-wide constraint that it cannot be in a "Complete" state with prospects.empty().
class PathDeciderSm
{
public:
    struct Output
    {
        std::vector< UniquePathDecider > prospects = {};
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
        using namespace kmap::com::db;
        using namespace kmap::sm::ev::detail;
        using namespace kmap::sm::state::detail;
        using namespace ranges;

        KM_RESULT_PROLOG();

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
            KM_RESULT_PROLOG();

            BC_ASSERT( output_ );

            auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

            return db->contains< HeadingTable >( ev.heading );
        };
        auto const has_prospect = [ & ]( auto const& ev ) -> bool
        {
            BC_ASSERT( output_ );

            for( auto const& decider : output_->prospects )
            {
                if( !decider.driver->is( boost::sml::state< kmap::sm::state::Error > ) )
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
            output_->prospects.back().driver->process_event( kmap::sm::ev::Fwd{} );
        };
        auto const start_selected_parent = [ & ]( auto const& ev ) -> void
        {
            KM_RESULT_PROLOG();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const parent = KTRYE( nw->fetch_parent( selected_node_ ) );
            output_->prospects.emplace_back( make_unique_path_decider( kmap_, root_, parent ) );
            output_->prospects.back().driver->process_event( kmap::sm::ev::Bwd{} );
        };
        auto const start_any_leads = [ & ]( auto const& ev ) -> void
        {
            BC_ASSERT( output_ );

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const filter_heading = views::filter( [ & ]( auto const& e )
            {
                auto const heading = KTRYE( nw->fetch_heading( e ) );
                return ev.heading == heading;
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
                output_->prospects.back().driver->process_event( kmap::sm::ev::Heading{ ev.heading } );
            }
        };
        auto const start_root = [ & ]( auto const& ev ) -> void
        {
            output_->prospects.emplace_back( make_unique_path_decider( kmap_, root_, root_ ) );
            output_->prospects.back().driver->process_event( kmap::sm::ev::Fwd{} );
        };
        auto const propagate = [ & ]( auto const& ev ) -> void
        {
            for( auto&& decider : output_->prospects )
            {
                decider.driver->process_event( ev );
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

        // Note: apparently, if a event isn't listed in the table below, it doesn't qualify for 'any'/`sml::_`.
        return make_transition_table
        (
        *   Start + fwd                          / start_selected                                = Next
        ,   Start + bwd     [ is_selected_root ] / set_error_msg( "path precedes root" )         = Error 
        ,   Start + bwd                          / start_selected_parent                         = Next
        ,   Start + heading [ heading_exists ]   / start_any_leads                               = Next
        ,   Start + heading                      / set_error_msg( "invalid heading" )            = Error
        ,   Start + root                         / start_root                                    = Next
        ,   Start + dis                          / set_error_msg( "invalid heading" )            = Error
        ,   Start + attrib                       / set_error_msg( "cannot start with attr" )     = Error
        ,   Start + tag                          / set_error_msg( "TODO: impl. start with tag" ) = Error
        ,   Start + any [ !is_anonymous ]                                                        = Error
        ,   Start + unexpected_event< boost::sml::_ >                                            = Error 

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

struct PathDecider
{
    std::shared_ptr< PathDeciderSm > sm;
    PathDeciderSmDriverPtr driver;
    PathDeciderSm::OutputPtr output;
};

auto make_path_decider( Kmap const& kmap
                      , Uuid const& root
                      , Uuid const& selected )
    -> PathDecider;

auto walk( Kmap const& kmap
         , Uuid const& root
         , Uuid const& selected
         , StringVec const& tokens )
    -> PathDecider;

} // namespace kmap

#endif // KMAP_PATH_SM_HPP
