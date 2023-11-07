/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path.hpp"

#include <com/database/db.hpp>
#include <com/network/network.hpp>
#include <com/tag/tag.hpp>
#include <com/visnetwork/visnetwork.hpp>
#include <common.hpp>
#include <contract.hpp>
#include <error/master.hpp>
#include <error/network.hpp>
#include <io.hpp>
#include <kmap.hpp>
#include <lineage.hpp>
#include <path/act/abs_path.hpp>
#include <path/act/order.hpp>
#include <path/node_view2.hpp>
#include <path/parser/tokenizer.hpp>
#include <path/sm.hpp>
#include <test/util.hpp>
#include <util/result.hpp>
#include <utility.hpp>

#include <boost/sml.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/action/remove_if.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/transform.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/tokenize.hpp>
#include <range/v3/view/transform.hpp>

#include <cstdlib>
#include <map>
#include <regex>

#if KMAP_LOGGING_ALL
    #define KMAP_LOGGING_PATH
#endif // KMAP_LOGGING_PATH_ALL

using namespace ranges;
namespace rvs = ranges::views;

namespace kmap {
namespace {

// TODO: It appears Boost.Contract causes heap errors inside of the lambdas inside the SM.
//       I haven't root caused this. It could be emscripten, Boost.Contract, or the underlying Clang compiler causing the issue.
//       In the meantime, I have commented out contracts in the SM lambdas.
// TODO: The problem with the present impl. is that it's trying to maintain two different states at once: the individual prospective path and the combined.
//       The impl. would be simpler if a single SM decided the individual path, and another decided the combined result.
struct [[ deprecated( "Use PathDeciderSm instead" ) ]] HeadingPathSm 
{
    // Valid Events.
    struct Fwd {};
    struct Bwd {};
    struct Tail {};
    struct CompleteTail {};

    // Exposed States.
    class CompleteState;
    class IncompleteState;
    class ErrorState;

    HeadingPathSm( Kmap const& kmap
                 , Uuid const& root_id
                 , Uuid const& selected_node )
        : kmap_{ kmap } 
        , root_id_{ root_id }
        , selected_node_{ selected_node }
    {
    }
    
    // Table
    // Expects tokenizing done prior to event processing: Delim{'.'|','|'*'} or Heading/Header.
    auto operator()()
    {
        using namespace boost::sml;
        using boost::sml::event;
    
        /* States */
        auto Start = state< class Start >;
        auto RootNode = state< class RootNode >;
        auto Travel = state< class Travel >;
        auto Node = state< class Node >;
        auto TailNode = state< class TailNode >;
        auto CompleteTailNode = state< class CompleteTailNode >;
        auto TailRoot = state< class TailRoot >;
        auto TailStart = state< class TailStart >;
        auto CompleteTailRoot = state< class CompleteTailRoot >;
        auto PathComplete = state< CompleteState >;
        auto PathIncomplete = state< IncompleteState >;
        auto Error = state< ErrorState >;

        /* Guards */
        auto exists = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

            return db->contains< com::db::HeadingTable >( ev );
        };

        auto is_root = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );

            return KTRYE( nw->fetch_heading( root_id_ ) ) == ev; // TODO: Does this fail when another node is named "root"?
        };

        auto is_selected_root = [ & ]( auto const& ev )
        {
            return selected_node_ == root_id_;
        };

        auto is_child = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto fn = [ & ]( auto const& e )
            {
                if( e.empty() )
                {
                    return false;
                }

                auto const parent = e.back();
                auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
                auto const child = nw->fetch_child( parent, ev );

                return bool{ child };
            };
            auto const it = find_if( paths()
                                   , fn );
        
            return it != end( paths() );
        };

        auto has_parent = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto fn = [ & ]( auto const& e )
            {
                BC_ASSERT( !e.empty() );

                auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
                auto const current = e.back();

                return nw->fetch_parent( current ).has_value();
            };
            auto const it = find_if( paths(), fn );
        
            return it != end( paths() );
        };

        // auto is_parent_root = [ & ]( Bwd const& ev )
        // {
        //     auto fn = [ & ]( auto const& e )
        //     {
        //         if( e.empty() )
        //         {
        //             return false;
        //         }

        //         return bool{ nw->fetch_parent( e.back() ) };
        //     };

        //     return 0 != count_if( paths()
        //                         , fn );
        // };

        auto is_path_existent = [ & ]( auto const& ev )
        {
            return 0 < count_if( paths()
                               , []( auto const& e ){ return !e.empty(); } );
        };

        auto empty_paths = [ & ]( auto const& ev )
        {
            (void)ev;

            return possible_paths_->empty();
        };


        /* Actions */
        auto push_root = [ & ]( auto const& ev )
        {
            // BC_CONTRACT()
            //     BC_PRE([ & ]
            //     {
            //         BC_ASSERT( is_root( ev ) );
            //         BC_ASSERT( paths().size() == 0 );
            //     })
            //     BC_POST([ & ]
            //     {
            //         BC_ASSERT( paths().size() == 1 );
            //     })
            // ;

            paths().emplace_back( UuidPath{ root_id_ } );
        };

        auto push_any_leads = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto filter = views::filter( [ & ]( auto const& e )
            {
                auto const heading = nw->fetch_heading( e );
                BC_ASSERT( heading );
                return ev == heading.value();
            } );
            auto const heading = ev;
            auto const all_ids = nw->fetch_nodes( heading );

            paths() = all_ids
                    | filter
                    | views::transform( [ & ]( auto&& e ){ return UuidPath{ e }; } )
                    | to< UuidPaths >();
        };

        auto push_headings = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            // BC_CONTRACT()
            //     BC_PRE([ & ]
            //     {
            //         BC_ASSERT( is_min_path_size( 1 ) );
            //     })
            //     BC_POST([ & ] 
            //     {
            //         BC_ASSERT( is_min_path_size( 2 ) );
            //         // TODO: include additional assertion that each path has
            //         // either grown by 1, or removed.
            //     })
            // ;
            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );

            for( auto&& path : paths() )
            {
                if( auto const id = nw->fetch_child( path.back(), ev )
                  ; id )
                {
                    path.emplace_back( id.value() );
                }
                else
                {
                    path = {};
                }
            }
            
            clear_empty();
        };

        // TODO: I'm not sure about the popping. 
        // It may be worthwhile to maintain a separate vector of directions { [ Fwd | Bwd ] }, and not do any popping, thereby being able to reproduce the input string accurately, for completion.
        // I can include additional actions (push_direction) to the state.
        auto const push_parents = [ & ]( auto const& ev )
        { 
            KM_RESULT_PROLOG();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );

            for( auto&& path : paths() )
            {
                BC_ASSERT( !path.empty() ); // Other pertinent actions should ensure empty paths are removed.

                io::print( "pushing parent of: {}\n", nw->fetch_heading( path.back() ).value() );

                auto const node = path.back();

                if( auto const parent = nw->fetch_parent( node )
                  ; parent )
                {
                    path.emplace_back( parent.value() );
                }
                else
                {
                    path = {};
                }
            }
            
            clear_empty();
        };

        auto const push_selected_parent = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );

            io::print( "pushing parent of selected: {}\n", nw->fetch_heading( selected_node_ ).value() );

            auto const parent = nw->fetch_parent( selected_node_ );

            BC_ASSERT( parent );

            paths().emplace_back( UuidPath{ parent.value() } );
        };

        auto push_all_children = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto new_paths = UuidPaths{};

            for( auto const& path : paths() )
            {
                // TODO: assert: !path.empty()
                for( auto const id : nw->fetch_children( path.back() ) )
                {
                    auto new_path = path;

                    new_path.emplace_back( id );
                    new_paths.emplace_back( new_path );
                }
            }

            paths() = new_paths;
        };

        auto push_parent_children = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto new_paths = UuidPaths{};

            for( auto const& path : paths() )
            {
                BC_ASSERT( !path.empty() );

                if( auto const parent = nw->fetch_parent( path.back() )
                  ; parent )
                {
                    BC_ASSERT( !parent );

                    for( auto const id : nw->fetch_children( parent.value() ) )
                    {
                        auto new_path = path;

                        new_path.emplace_back( id );
                        new_paths.emplace_back( new_path );
                    }
                }
            }

            paths() = new_paths;
        };

        // auto pop_headings = [ & ]( auto const& ev )
        // {
        //     // BC_CONTRACT()
        //     //     BC_PRE([ & ]
        //     //     {
        //     //         BC_ASSERT( is_min_path_size( 1 ) );
        //     //     })
        //     //     BC_POST([ & ]
        //     //     {
        //     //         BC_ASSERT( is_min_path_size( 1 ) );
        //     //     })
        //     // ;
            
        //     for( auto&& path : paths() )
        //     {
        //         BC_ASSERT( !path.empty() );

        //         if( path.size() > 1 )
        //         {
        //             path.pop_back();
        //         }
        //         else
        //         {
        //             // auto const back = path.back();

        //             // TODO:
        //             // pop tail, push parent. assert has parent
        //         }
        //     }
        // };

        auto complete_root = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            // BC_CONTRACT()
            //     BC_POST([ & ]
            //     {
            //         // BC_ASSERT( !possible_paths_.empty() );
            //     })
            // ;
            
            auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
            auto const known = nw->fetch_heading( root_id_ ).value();
            auto const ml = match_length( ev
                                        , known );

            if( ml == ev.size() )
            {
                paths().emplace_back( UuidPath{ root_id_ } );
            }
        };

        auto complete_any = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            // BC_CONTRACT()
            //     BC_POST([ & ]
            //     {
            //         // BC_ASSERT( !possible_paths_.empty() );
            //     })
            // ;

            auto const nw = KTRYE( kmap_.fetch_component< com::Network>() );
            auto const db = KTRYE( kmap_.fetch_component< com::Database >() );
            // auto const& headings = db.fetch_headings().get< 2 >();
            auto const& ht = db->fetch< com::db::HeadingTable >().underlying();
            auto const& hv = ht.get< com::db::right_ordered >();
            auto matches = std::vector< UuidPath >{};
            
            for( auto it = hv.lower_bound( ev )
               ; it != hv.end()
               ; ++it )
            {
                auto const nid = it->left();
                auto const heading = it->right();

                if( heading.compare( 0, ev.size(), ev ) != 0 )
                {
                    break;
                }

                auto const node_and_aliases = [ & ]
                {
                    auto all = nw->alias_store().fetch_aliases( com::AliasItem::rsrc_type{ nid } );

                    all.emplace( nid );

                    return all;
                }();

                for( auto const& e : node_and_aliases )
                {
                    if( nw->is_lineal( root_id_, Uuid{ e } ) )
                    {
                        matches.emplace_back( UuidPath{ e } );
                    }
                }
            }

            paths() = matches;
        };

        auto complete_path = [ & ]( auto const& ev )
        {
            KM_RESULT_PROLOG();

            // BC_CONTRACT()
            //     BC_PRE([ & ]
            //     {
            //         BC_ASSERT( is_min_path_size( 1 ) );
            //     })
            //     BC_POST([ & ]
            //     {
            //         BC_ASSERT( is_min_path_size( 2 ) );
            //     })
            // ;

            auto const nw = KTRYE( kmap_.fetch_component< com::Network>() );

            auto new_paths = [ & ]
            {
                auto rv = std::vector< UuidPath >{};

                for( auto const& path : paths() )
                {
                    auto const cids = nw->fetch_children( path.back() );

                    for( auto const cid : cids )
                    {
                        if( ev.size() == match_length( ev, KTRYE( nw->fetch_heading( nw->resolve( cid ) ) ) ) )
                        {
                            rv.emplace_back( path )
                              .emplace_back( cid );
                        }
                    }
                }

                return rv;
            }();

            paths() = new_paths;
        };

        auto set_heading_error = [ & ]( std::string const& msg )
        {
            return [ & ]( auto const& ev ) mutable
            {
                error_msg() = fmt::format( "{}: {}"
                                         , msg
                                         , ev );
            };
        };

        // auto set_error = [ & ]( std::string const& msg )
        // {
        //     return [ & ]( auto const& ev ) mutable
        //     {
        //         (void)ev;

        //         error_msg() = msg;
        //     };
        // };

        // TODO: what to do about unexpected events? Can I configure s.t. an
        // assertion is raised? I'd prefer not to have to encode a event< _ >
        // for each state.
        return make_transition_table
        (
        *   Start + event< Tail >
                = TailStart
        ,   Start + event< CompleteTail >
                = CompleteTailNode
        ,   Start + event< Heading > [ exists ] / push_any_leads
                = Travel
        ,   Start + event< Heading > [ !exists ] / set_heading_error( "node not found" )
                = Error
        ,   Start + event< Fwd >
                = RootNode
        ,   Start + event< Bwd > [ !is_selected_root ] / push_selected_parent
                = Travel 
        ,   Start + event< Bwd > [ is_selected_root ] // / set_heading_error( "path precedes root" )
                = Error

        ,   TailStart + event< Heading > [ empty_paths && exists ] / push_any_leads // TODO: This seems precarious. Relying on no paths being present at this point doesn't take into account the popping of Bwd events.
                = PathComplete
        ,   TailStart + event< Heading > [ is_child ] / push_headings
                = PathComplete
        ,   TailStart + event< Heading > [ !is_child ] 
                = PathIncomplete
        ,   TailStart + event< Fwd >
                = PathIncomplete 
        ,   TailStart + event< Bwd > [ !is_selected_root ] / push_selected_parent
                = PathComplete 
        ,   TailStart + event< Bwd > [ is_selected_root ] // / set_heading_error( "path precedes root" )
                = Error

        ,   Travel + event< Fwd >
                = Node
        ,   Travel + event< Bwd > / push_parents
                = Node
        ,   Travel + event< Tail >
                = TailNode
        ,   Travel + event< CompleteTail >
                = CompleteTailNode

        ,   RootNode + event< Heading > [ is_root ] / push_root
                = Travel
        ,   RootNode + event< Heading > [ !is_root ] / set_heading_error( "root not found" )
                = Error
        ,   RootNode + event< Tail >
                = TailRoot
        ,   RootNode + event< CompleteTail >
                = CompleteTailRoot

        ,   Node + event< Heading > [ is_child ] / push_headings
                = Travel
        ,   Node + event< Heading > [ !is_child ] / set_heading_error( "path node not found" )
                = Error
        ,   Node + event< Tail >
                = TailNode
        ,   Node + event< CompleteTail >
                = CompleteTailNode
        ,   Node + event< Bwd > / push_parents
                = Node 

        ,   TailNode + event< Heading > [ empty_paths && exists ] / push_any_leads // TODO: This seems precarious. Relying on no paths being present at this point doesn't take into account the popping of Bwd events.
                = PathComplete
        ,   TailNode + event< Heading > [ is_child ] / push_headings
                = PathComplete
        ,   TailNode + event< Heading > [ !is_child ] 
                = PathIncomplete
        ,   TailNode + event< Fwd >
                = PathIncomplete 
        ,   TailNode + event< Bwd > [ has_parent ] / push_parents
                = PathComplete 
        ,   TailNode + event< Bwd > [ !has_parent ]// / push_parents
                = PathIncomplete 

        ,   CompleteTailNode + event< Heading > [ is_path_existent ] / complete_path
                = PathComplete
        ,   CompleteTailNode + event< Heading > [ !is_path_existent ] / complete_any
                = PathComplete
        ,   CompleteTailNode + event< Fwd > / push_all_children
                = PathComplete 
        ,   CompleteTailNode + event< Bwd > / push_parent_children
                = PathComplete 

        ,   TailRoot + event< Heading > [ is_root ] / push_root
                = PathComplete
        ,   TailRoot + event< Heading > [ !is_root ] / set_heading_error( "root not found" )
                = Error

        ,   CompleteTailRoot + event< Heading > / complete_root
                = PathComplete
        );
    }

    auto is_min_path_size( uint32_t const& n )
        -> bool
    {
        auto const count = count_if( paths() 
                                   , [ &n ]( auto const& e ){ return e.size() < n; } );

        return count == 0;
    }

    auto prospective_paths() const
        -> std::vector< UuidPath > const&
    {
        return *possible_paths_;
    }

    auto error() const
        -> std::string

    {
        return *error_msg_;
    }

protected:
    using UuidPaths = std::vector< UuidPath >;
    // enum Direction { bwd, fwd };
    // using PathDirections = std::vector< std::vector< Direction > >;

    auto paths()
        -> UuidPaths&

    {
        return *possible_paths_;
    }

    auto clear_empty()
        -> void
    {
        *possible_paths_ |= actions::remove_if( [ & ]( auto const& e ){ return e.empty(); } );
    }

    auto error_msg()
        -> std::string

    {
        return *error_msg_;
    }

private:
    Kmap const& kmap_;
    Uuid const root_id_;
    Uuid const selected_node_;
    std::shared_ptr< UuidPaths > possible_paths_ = std::make_shared< UuidPaths >();
    // std::shared_ptr< PathDirections > possible_path_directions_ = std::make_shared< PathDirections >(); // TODO: Or should this be paired with the path? If I know for certain that every pushed path has a unambiguous direction, that would make the most sense.
    std::shared_ptr< std::string > error_msg_ = std::make_shared< std::string >();
};

} // anonymous namespace

auto operator<( CompletionNode const& lhs
              , CompletionNode const& rhs ) // TODO: =default when compiler supports.
    -> bool
{
    // Acounts for case where lhs.x == rhs.x
    if( lhs.target < rhs.target ) return true;
    if( rhs.target < lhs.target ) return false;
    if( lhs.path < rhs.path ) return true;
    if( rhs.path < lhs.path ) return false;
    return lhs.disambig < rhs.disambig;
}

auto tokenize_path( std::string const& raw )
    -> StringVec
{
    // I think I can temporarily go from string to ast to tokens 
    auto rv = StringVec{};
    auto const rgx = std::regex{ R"([\.\,\/\']|[^\.\,\/\']+)" }; // .,/' or one or more of none of these: heading.

    rv = raw
       | views::tokenize( rgx )
       | to< StringVec >();

    return rv;
}

auto absolute_path( Kmap const& km
                  , Uuid const& desc )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "desc", desc );

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto nv = UuidVec{ desc };

    auto n = desc;

    while( n != km.root_node_id() )
    {
        n = KTRY( nw->fetch_parent( n ) );

        nv.emplace_back( n );
    }

    rv = nv | views::reverse | ranges::to< UuidVec >();

    return rv;
}

auto absolute_path_flat( Kmap const& km
                       , Uuid const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", node );

    auto rv = KMAP_MAKE_RESULT( std::string );

    if( km.root_node_id() == node )
    {
        rv = KTRY( anchor::abs_root
                 | act2::abs_path_flat( km ) );
    }
    else if( auto const attr_root = anchor::node( node )
                                  | view2::left_lineal( "$" ) // TODO: Foolproof to fetch attr_root? Maybe... []( const& n ){ return nw->is_attr_root( n ); } ???
                                  | act2::fetch_node( km )
      ; attr_root )
    {
        auto const nw = KTRY( km.fetch_component< com::Network >() );

        if( "$" == KTRY( nw->fetch_heading( node ) ) )
        {
            rv = KTRY( anchor::node( attr_root.value() )
                     | act2::abs_path_flat( km ) );
        }
        else
        {
            rv = KTRY( anchor::node( attr_root.value() )
                     | view2::desc( node )
                     | act2::abs_path_flat( km ) );
        }
    }
    else
    {
        rv = KTRY( anchor::abs_root
                 | view2::desc( node )
                 | act2::abs_path_flat( km ) );
    }

    return rv;
}

auto complete_any( Kmap const& kmap
                 , Uuid const& root
                 , std::string const& heading )
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( UuidSet );

    KMAP_ENSURE( is_valid_heading( heading ), error_code::network::invalid_heading );

    auto const nw = KTRY( kmap.fetch_component< com::Network>() );
    auto const db = KTRY( kmap.fetch_component< com::Database >() );
    auto const& ht = db->fetch< com::db::HeadingTable >().underlying();
    auto const& hv = ht.get< com::db::right_ordered >();
    auto matches = UuidSet{};
    
    for( auto it = hv.lower_bound( heading )
       ; it != hv.end()
       ; ++it )
    {
        auto const& nid = it->left();
        auto const& nh = it->right();

        if( nh.compare( 0, heading.size(), heading ) != 0 )
        {
            break;
        }

        auto const node_and_aliases = [ & ]
        {
            auto all = nw->alias_store().fetch_aliases( com::AliasItem::rsrc_type{ nid } );

            all.emplace( nid );

            return all;
        }();

        for( auto const& e : node_and_aliases )
        {
            if( nw->is_lineal( root, e ) )
            {
                matches.emplace( e );
            }
        }
    }

    rv = matches;

    return rv;
}

// TODO: I reckon the completion logic is sufficiently complex to warrant it's own SM, leveraging the output of the existing decide_path().
//       That is, a single SM corresponding to each prospect. I'm a bit trepidatious about actually implementing as an SM given the strange
//       behavior related to Boost.SML SMs experienced in the past.
auto complete_path( Kmap const& kmap
                  , Uuid const& root
                  , Uuid const& selected
                  , std::string const& raw )
    -> Result< CompletionNodeSet >
{
    using Set = CompletionNodeSet;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );
        KM_RESULT_PUSH_STR( "raw", raw );

    auto rv = KMAP_MAKE_RESULT( Set );

    BC_CONTRACT()
        BC_POST( [ & ]
        {
            if( rv )
            {
                for( auto const val = rv.value(); auto const& e : val ) { BC_ASSERT( e.path.size() >= raw.size() ); }
            }
        } )
    ;

    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    KMAP_ENSURE( nw->is_lineal( root, selected ), error_code::network::invalid_lineage );

    auto const tokens = ast::path::to_string_vec( KTRY( parser::path::tokenize_heading_path( raw ) ) );

    if( !tokens.empty() )
    {
        auto comps = Set{};

        if( is_valid_heading( tokens.back() ) ) // ends in heading
        {
            if( tokens.size() == 1 )
            {
                auto const nodes = KMAP_TRY( complete_any( kmap, root, raw ) );
                comps = nodes
                      | views::transform( [ & ]( auto const& e ){ return CompletionNode{ .target=e, .path=KTRYE( nw->fetch_heading( e ) ) }; } )
                      | to< Set >();
            }
            else
            {
                auto decider = walk( kmap, root, selected, tokens | views::drop_last( 1 ) | to< StringVec >() );

                for( auto&& prospect : decider.output->prospects )
                {
                    if( prospect.driver->is( boost::sml::state< sm::state::FwdNode > ) )
                    {
                        for( auto const& child : nw->fetch_children( prospect.output->prospect.back() ) )
                        {
                            auto const ch = nw->fetch_heading( child ).value();

                            if( ch.starts_with( tokens.back() ) )
                            {
                                auto const joined = tokens | views::drop_last( 1 ) | views::join | to< std::string >();
                                auto const np = joined + ch;
                                comps.emplace( CompletionNode{ .target=child, .path=np } );
                            }
                        }
                    }
                    else if( prospect.driver->is( boost::sml::state< sm::state::BwdNode > ) )
                    {
                        BC_ASSERT( !prospect.output->prospect.empty() );

                        auto const p = prospect.output->prospect.back();
                        auto const ph = nw->fetch_heading( p ); BC_ASSERT( ph );

                        if( ph.value().starts_with( tokens.back() ) )
                        {
                            auto const joined = tokens | views::drop_last( 1 ) | views::join | to< std::string >();
                            auto const np = joined + ph.value();
                            comps.emplace( CompletionNode{ .target=p, .path=np } );
                        }
                    }
                    else if( prospect.driver->is( boost::sml::state< sm::state::DisNode > ) )
                    {
                        BC_ASSERT( !prospect.output->disambiguation.empty() );

                        auto const p = KTRY( nw->fetch_parent( prospect.output->disambiguation.back() ) );
                        auto const ph = KTRY( nw->fetch_heading( p ) );

                        if( ph.starts_with( tokens.back() ) )
                        {
                            auto const joined = tokens | views::drop_last( 1 ) | views::join | to< std::string >();
                            auto const np = joined + ph;
                            auto const target = prospect.output->prospect.back();
                            BC_ASSERT( !prospect.output->disambiguation.empty() );
                            auto const disp = KTRY( nw->fetch_parent( prospect.output->disambiguation.back() ) );
                            comps.emplace( CompletionNode{ .target=target, .path=np, .disambig={ disp } } );
                        }
                    }
                }
            }

            // Append append ,.
            auto tails = Set{};
            for( auto const& comp : comps )
            {
                if( KTRY( nw->fetch_heading( comp.target ) ) == tokens.back() )
                {
                    auto const joined = tokens | views::join | to< std::string >();

                    if( auto const parent = nw->fetch_parent( comp.target )
                      ; parent && comp.target != root )
                    {
                        tails.emplace( CompletionNode{ .target=parent.value(), .path=fmt::format( "{},", joined ) } );
                    }

                    if( !nw->fetch_children( comp.target ).empty() )
                    {
                        tails.emplace( CompletionNode{ .target=comp.target, .path=fmt::format( "{}.", joined ) } );
                    }
                }
            }
            // Append disambig cases.
            auto disams = Set{};
            {
                auto const filter_exact_headings = [ & ]( auto const& m )
                {
                    return m 
                         | views::filter( [ & ]( auto const& e )
                           {
                               auto const target = e.disambig.empty() ? e.target : e.disambig.back();
                               return tokens.back() == nw->fetch_heading( target ).value();
                           } ) 
                         | to< CompletionNodeSet >();
                };
                auto const filter_ambig = [ & ]( auto const& m )
                {
                    return m 
                         | views::filter( [ & ]( auto const& e ) { return ranges::count( m, e.path, &CompletionNode::path ) > 1; } ) 
                         | to< CompletionNodeSet >();
                };
                auto const filter_unambig = [ & ]( auto const& m )
                {
                    return m 
                         | views::filter( [ & ]( auto const& e ){ return ranges::count( m, e.path, &CompletionNode::path ) == 1; } )
                         | to< CompletionNodeSet >();
                };
                auto const do_disambig = [ & ]( auto const& m )
                {
                    return m
                         | views::transform( [ & ]( auto const& e )
                           {
                               auto const tdis = e.disambig.empty() ? e.target : e.disambig.back();
                               auto const tdisp = KTRYE( nw->fetch_parent( tdis ) );
                               auto ndis = e.disambig;
                               ndis.emplace_back( tdisp );
                               return CompletionNode{ .target=e.target, .path=io::format( "{}'{}", e.path, KTRYE( nw->fetch_heading( tdisp ) ) ), .disambig=ndis };
                           } )
                         | to< CompletionNodeSet >();
                };
                auto const comp_set = filter_exact_headings( comps );
                auto ambigs = comp_set;

                while( !ambigs.empty() )
                {
                    ambigs = filter_ambig( ambigs );
                    ambigs = do_disambig( ambigs );
                    auto const unambigs = filter_unambig( ambigs );

                    disams.insert( ambigs.begin(), ambigs.end() );
                }
            }

            comps.insert( tails.begin(), tails.end() );
            comps.insert( disams.begin(), disams.end() );
        }
        else // ends in non-heading.
        {
            auto decider = walk( kmap, root, selected, tokens );

            for( auto&& prospect : decider.output->prospects )
            {
                if( prospect.driver->is( boost::sml::state< sm::state::FwdNode > ) )
                {
                    for( auto const& child : nw->fetch_children( prospect.output->prospect.back() ) )
                    {
                        auto const joined = tokens | views::join | to< std::string >();
                        auto const np = joined + nw->fetch_heading( child ).value();

                        comps.emplace( CompletionNode{ .target=child, .path=np } );
                    }
                }
                else if( prospect.driver->is( boost::sml::state< sm::state::BwdNode > ) )
                {
                    auto const target = prospect.output->prospect.back();
                    auto const joined = tokens | views::join | to< std::string >();
                    auto const np = joined + nw->fetch_heading( target ).value();

                    comps.emplace( CompletionNode{ .target=target, .path=np } );
                }
                else if( prospect.driver->is( boost::sml::state< sm::state::DisNode > ) )
                {
                    auto const target = prospect.output->disambiguation.back();
                    auto const p = KTRYE( nw->fetch_parent( target ) );
                    auto const joined = tokens | views::join | to< std::string >();
                    auto const np = joined + KTRYE( nw->fetch_heading( p ) );

                    comps.emplace( CompletionNode{ .target=target, .path=np } );
                }
            }
        }

        rv = comps;
    }

    return rv;
}

auto complete_child_heading( Kmap const& kmap
                           , Uuid const& parent
                           , Heading const& heading )
    -> IdHeadingSet
{
    KM_RESULT_PROLOG();

    auto rv = IdHeadingSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const children = nw->fetch_children( parent );

    for( auto const& cid : children )
    {
        auto const cheading = KTRYE( nw->fetch_heading( cid ) );
        if( heading.size() == match_length( heading, cheading ) )
        {
            rv.emplace( cid, cheading );
        }
    }

    return rv;
}

/* Attempts to complete 'path'.
 *     If successful, returns completed paths.
 *     If unsuccessful, returns completed paths with nearest matching prefix.
 */
auto complete_path_reducing( Kmap const& kmap
                           , Uuid const& root
                           , Uuid const& selected
                           , std::string const& path )
    -> Result< CompletionNodeSet >
{
    using Set = CompletionNodeSet;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );
        KM_RESULT_PUSH_STR( "path", path );

    auto rv = KMAP_MAKE_RESULT( Set );

    if( is_valid_heading_path( path ) )
    {
        auto reducing = path;
        auto ps = KMAP_TRY( complete_path( kmap, root, selected, reducing ) );

        while( ps.empty()
            && reducing.size() > 0 )
        {
            reducing = reducing | views::drop_last( 1 ) | to< std::string >();
            ps = KMAP_TRY( complete_path( kmap, root, selected, reducing ) );
        }

        rv = ps;
    }

    return rv;
}

auto decide_path( Kmap const& kmap
                , Uuid const& root
                , Uuid const& selected
                , StringVec const& tokens ) // TODO: If StringVec is made into HeadingVec, with constraints that Heading must be composed of valid chars, there'd be no need to check the contents for validity.
    -> Result< UuidVec >
{
    using boost::sml::logger;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const decider = walk( kmap
                             , root
                             , selected
                             , tokens );

    if( decider.driver->is( boost::sml::state< sm::state::Error > ) )
    {
        auto const joined = tokens | views::join | to< std::string >();
        rv = KMAP_MAKE_ERROR_MSG( error_code::node::invalid_path, io::format( "\n\terror: {}\n\theading: {}", decider.output->error_msg, joined ) );
    }
    else
    {
        rv = decider.output->prospects
           | views::filter( []( auto const& e ){ return !e.driver->is( boost::sml::state< sm::state::Error > ); } ) // Note: b/c the SM allows a "delay" - perserving the failure for at least one cycle, must filter out manually.
           | views::transform( []( auto const& e ){ BC_ASSERT( e.output && !e.output->prospect.empty() ); return e.output->prospect.back(); } )
           | to< UuidVec >();
    }

    return rv;
}

/**
 * Returns all matches (if more than one implies ambiguous) that could be resolved from the path given by "raw".
 **/
auto decide_path( Kmap const& kmap
                , Uuid const& root
                , Uuid const& selected
                , std::string const& raw )
    -> Result< UuidVec > 
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );
        KM_RESULT_PUSH_STR( "raw", raw );

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const nw = KTRY( kmap.fetch_component< com::Network > () );

    KMAP_ENSURE( !raw.empty(), error_code::node::invalid_heading );
    KMAP_ENSURE_MSG( is_valid_heading_path( raw ), error_code::node::invalid_heading, raw );
    KMAP_ENSURE_MSG( nw->is_lineal( root, selected ), error_code::node::not_lineal, io::format( "root `{}` not lineal to selected `{}`\n", KTRYE( absolute_path_flat( kmap, root ) ), KTRYE( absolute_path_flat( kmap, selected ) ) ) );

    auto const tokens = tokenize_path( raw );

    rv = KMAP_TRY( decide_path( kmap
                              , root
                              , selected
                              , tokens ) );

    return rv;
}

SCENARIO( "decide_path", "[network][path]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network", "tag_store" );

    auto& km = Singleton::instance();
    auto nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto tstore = REQUIRE_TRY( km.fetch_component< com::TagStore >() );
    auto const rn = nw->root_node();
    auto const check = [ & ]( auto const& root
                            , auto const& selected
                            , auto const& ipath
                            , std::vector< std::string > const& abs_opaths ) -> bool
    {
        auto decided_set = std::set< Uuid >{};
        auto expected = std::set< Uuid >{};

        if( auto const decided = decide_path( km, root, selected, ipath )
          ; decided )
        {
            decided_set = decided.value() | ranges::to< std::set >();

            for( auto const& opath : abs_opaths )
            {
                auto const n = REQUIRE_TRY( decide_path( km, rn, rn, opath ) );

                REQUIRE( n.size() == 1 );

                expected.emplace( n.at( 0 ) );
            }
        }

        return decided_set == expected;
    };

    GIVEN( "/" )
    {

        REQUIRE( check( rn, rn, "", {} ) );
        REQUIRE( check( rn, rn, "/", { "/" } ) );
        REQUIRE( check( rn, rn, ".", { "/" } ) );
        REQUIRE( check( rn, rn, ",", {} ) );
        REQUIRE( check( rn, rn, "'", {} ) );
        REQUIRE( check( rn, rn, "#", {} ) );

        GIVEN( "/#test" )
        {
            auto const t = REQUIRE_TRY( tstore->create_tag( "test" ) );

            REQUIRE_TRY( tstore->tag_node( rn, t ) );
        }

        GIVEN( "/1" )
        {
            auto const n1 = REQUIRE_TRY( nw->create_child( rn, "1" ) );

            REQUIRE( check( rn, rn, "", {} ) );
            REQUIRE( check( rn, rn, "/", { "/" } ) );
            REQUIRE( check( rn, rn, "/1", { "/1" } ) );
            REQUIRE( check( rn, rn, ".", { "/" } ) );
            REQUIRE( check( rn, rn, ".1", { "/1" } ) );
            REQUIRE( check( rn, rn, ",", {} ) );
            REQUIRE( check( rn, rn, "'", {} ) );
            REQUIRE( check( rn, rn, "#", {} ) );
            REQUIRE( check( rn, rn, "1", { "/1" } ) );

            REQUIRE( check( rn, n1, "", {} ) );
            REQUIRE( check( rn, n1, "/", { "/" } ) );
            REQUIRE( check( rn, n1, "/1", { "/1" } ) );
            REQUIRE( check( rn, n1, ".", { "/1" } ) );
            REQUIRE( check( rn, n1, ".1", {} ) );
            REQUIRE( check( rn, n1, ",", { "/" } ) );
            REQUIRE( check( rn, n1, "'", {} ) );
            REQUIRE( check( rn, n1, "#", {} ) );
            REQUIRE( check( rn, n1, "1", { "/1" } ) );

            REQUIRE( check( n1, n1, "", {} ) );
            REQUIRE( check( n1, n1, "/", { "/1" } ) );
            REQUIRE( check( n1, n1, "/1", {} ) );
            REQUIRE( check( n1, n1, ".", { "/1" } ) );
            REQUIRE( check( n1, n1, ".1", {} ) );
            REQUIRE( check( n1, n1, ",", {} ) );
            REQUIRE( check( n1, n1, "'", {} ) );
            REQUIRE( check( n1, n1, "#", {} ) );
            REQUIRE( check( n1, n1, "1", { "/1" } ) );

            GIVEN( "/1#test" )
            {
                auto const t = REQUIRE_TRY( tstore->create_tag( "test" ) );

                REQUIRE_TRY( tstore->tag_node( n1, t ) );
            }
        }
    }
}

// TODO: This seems to be a misnomer, or, at least, a misleading name. The "descendants" refers to "root_id" given,
//       but such a convention can be easily understood to be the descendants of the path instead.
auto fetch_descendants( Kmap const& kmap
                      , Uuid const& root_id
                      , Uuid const& selected_node
                      , std::string const& raw )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();

    return decide_path( kmap
                      , root_id
                      , selected_node
                      , raw );
}

auto fetch_descendants( Kmap const& kmap
                      , Uuid const& root )
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );

    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto rs = UuidSet{};

    if( auto const children = nw->fetch_children( root )
      ; !children.empty() )
    {
        for( auto const& child : children )
        {
            auto const fds = KTRY( fetch_descendants( kmap
                                                    , child
                                                    , []( auto const& e ){ (void)e; return true; } ) );
            
            rs.insert( fds.begin(), fds.end() );
        }
    }
    
    return rs;
}

auto has_geometry( Kmap const& kmap
                 , Uuid const& parent 
                 , std::regex const& geometry )
    -> bool
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    for( auto const& child : nw->fetch_children( parent ) )
    {
        auto const heading = KTRYE( nw->fetch_heading( child ) );
        auto result = std::smatch{};

        if( std::regex_match( heading
                            , result
                            , geometry ) )
        {
            return true;
        }
    }

    return false;
}

// TODO: Shouldn't this limit the root to a range, not absolute root? That is, say a path is completed (subroot, heading), then fed here.
//       This makes no account of such a subroot, only of the absolute root.
auto disambiguate_paths( Kmap const& kmap
                       , CompletionNodeSet const& node_set )
    -> Result< std::map< Uuid, std::string > >
{
    using ResMap = std::map< Uuid, std::string >;

    KM_RESULT_PROLOG();

    struct Node
    {
        std::string path = {};
        Uuid disambig = {};
        CompletionNode comp = {};
    };

    auto rv = KMAP_MAKE_RESULT( ResMap );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const disn = node_set
                    | views::transform( []( auto const& e ){ return Node{ e.path, e.target, e }; } )
                    | to< std::vector< Node > >();
    auto is_duplicate = [ & ]( auto const& collection )
    {
        return [ & ]( auto const& e )
        {
            return count_if( collection, [ & ]( auto const& e2 ){ return e2.path == e.path; } ) > 1;
        };
    };
    auto duplicates = disn
                    | views::filter( is_duplicate( disn ) )
                    | to_vector;
    auto uniques = disn
                 | views::remove_if( is_duplicate( disn ) )
                 | to_vector;

    while( !duplicates.empty() )
    {
        for( auto const& dup : duplicates )
        {
            KMAP_ENSURE( kmap.root_node_id() != dup.disambig, error_code::network::invalid_node );
        }
        auto const expanded = duplicates
                            | views::transform( [ & ]( auto const& e )
                            {
                                     io::print( "disambiguating: {}\n", e.comp.path );
                                 if( e.path.ends_with( '.' ) ) 
                                 {
                                    auto const p = nw->fetch_parent( e.disambig ).value();
                                    auto const ph = nw->fetch_heading( p ).value();

                                    return Node{ io::format( "{}'{}."
                                                           , e.path | views::drop_last( 1 ) | to< std::string >() // TODO: drop_last_while( '.' ) to remove all ending '.' if multiple?
                                                           , ph )
                                               , p
                                               , e.comp };
                                 }
                                 else if( e.path.ends_with( ',' ) )
                                 {
                                    auto const p = nw->fetch_parent( e.disambig ).value(); // TODO: Once input is in the form of UuidPath, use *( target.end() - 2 ) to get parent.
                                    auto const ph = nw->fetch_heading( p ).value();

                                    return Node{ io::format( "{}'{},"
                                                           , e.path | views::drop_last( 1 ) | to< std::string >() // TODO: drop_last_while( '.' ) to remove all ending '.' if multiple?
                                                           , ph )
                                               , p
                                               , e.comp };
                                 }
                                 else if( e.path.ends_with( '\'' ) )
                                 {
                                     io::print( "disambiguating ending in': {}\n", e.path );
                                    auto const p = nw->fetch_parent( e.disambig ).value();
                                    auto const ph = nw->fetch_heading( p ).value();

                                    return Node{ io::format( "{}{}"
                                                           , e.path
                                                           , ph )
                                               , p
                                               , e.comp };
                                 }
                                 else
                                 {
                                    auto const p = nw->fetch_parent( e.disambig ).value();
                                    auto const ph = nw->fetch_heading( p ).value();

                                    return Node{ io::format( "{}'{}"
                                                           , e.path
                                                           , ph )
                                               , p
                                               , e.comp };
                                 }
                            } )
                            | to_vector;

        duplicates = expanded
                   | views::filter( is_duplicate( expanded ) ) 
                   | to_vector;

        auto const new_uniques = expanded 
                               | views::remove_if( is_duplicate( expanded ) )
                               | to_vector;

        uniques.insert( uniques.end()
                      , new_uniques.begin(), new_uniques.end() );
    }

    rv = uniques
       | views::transform( []( auto const& e ){ return std::pair{ e.comp.target, e.path }; } )
       | to< ResMap >();

    return rv;
}

auto disambiguate_path( Kmap const& km
                      , Uuid const& node )
    -> Result< std::map< Uuid, Uuid > >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::map< Uuid, Uuid > >();

    BC_CONTRACT()
        BC_POST([ & ]
        {
            // TODO: Uncomment. Should pass, but doesn't in postcond, it does at end of function. Easily observe by place the assert after rv assigned.
            //       Again, contracts behaving strangely. Worrying.
            // if( rv )
            // {
                // fmt::print( "[post] input node:\n" );
                // BC_ASSERT( print_tree( km, node ) );
                // for( auto const& [ k, v ] : rv.value() )
                // {
                //     fmt::print( "[post] output node:\n" );
                //     BC_ASSERT( print_tree( km, k ) );
                // }
                // std::cout.flush();
                // BC_ASSERT( rv.value().contains( node ) );
            // }
        })
    ;

    auto const db = KTRY( km.fetch_component< com::Database >() );
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto const heading = KTRY( nw->fetch_heading( node ) );
    auto const ambig_nodes = [ & ]
    {
        auto const nonaliases = db->fetch_nodes( heading );
        auto combined = nonaliases;
        for( auto const& e : nonaliases )
        {
            auto const dsts = nw->alias_store().fetch_aliases( com::AliasItem::rsrc_type{ e } );
            
            combined.insert( dsts.begin(), dsts.end() );
        }
        return combined;
    }();

    auto const lineages = ambig_nodes
                        | rvs::transform( [ & ]( auto const& n ){ return anchor::node( n )
                                                                       | view2::left_lineal
                                                                       | view2::order
                                                                       | act2::to_node_vec( km ); } )
                        | ranges::to< std::vector< std::vector< Uuid > > >();

    rv = KTRY( disambiguate_paths2( km, lineages ) );

    return rv;
}

// I suspect it's possible to get conflicting disambig headings as a result of this, but considering it "good enough" for now.
auto disambiguate_paths2( Kmap const& km
                        , std::vector< std::vector< Uuid > > const& lineages )
    -> Result< std::map< Uuid, Uuid > >
{
    KM_RESULT_PROLOG();

    for( auto const& lineage : lineages )
    {
        KMAP_ENSURE( !lineage.empty(), error_code::common::uncategorized );

        // Ensure "lineage" are lineal.
        {
            auto const nw = KTRY( km.fetch_component< com::Network >() );
            auto pit = lineage.begin();
            auto cit = std::next( pit );

            while( cit != lineage.end() )
            {
                KMAP_ENSURE( is_parent( *nw, *pit, *cit ), error_code::common::uncategorized );

                pit = cit;
                cit = std::next( pit );
            }
        }

        KM_RESULT_PUSH( "lineage", lineage.back() );
    }

    auto rv = result::make_result< std::map< Uuid, Uuid > >();
    auto rmap = std::map< Uuid, Uuid >{};

    if( lineages.size() == 0 )
    {
        rv = rmap;
    }
    else if( lineages.size() == 1 )
    {
        auto const& lineage = lineages.at( 0 );

        rmap.emplace( std::pair{ lineage.back(), lineage.front() } );
    }
    else
    {
        BC_ASSERT( !lineages.empty() && !lineages[ 0 ].empty() );
        auto const common_root = lineages[ 0 ][ 0 ]; // Ensured to exist by code above.

        for( auto const& lineage : lineages )
        {
            BC_ASSERT( !lineage.empty() );
            KMAP_ENSURE( lineage[ 0 ] == common_root, error_code::common::uncategorized );
        }

        auto mlineages = lineages;

        // Pop lineages of size 2. root.target only has "root" as a possible option for disambig.
        {
            for( auto it = mlineages.begin()
               ; it != mlineages.end()
               ; )
            {
                if( it->size() == 2 )
                {
                    BC_ASSERT( !it->empty() );
                    rmap.emplace( std::pair{ it->back(), it->front() } );

                    it = mlineages.erase( it );
                }
                else
                {
                    ++it;
                }
            }
        }
        // Process remaining
        {
            {
                // Drop roots
                for( auto&& lin : mlineages )
                {
                    // lin.pop_front();
                    BC_ASSERT( !lin.empty() );
                    lin.erase( lin.begin() );
                }

                auto root_groups = std::map< Uuid, std::vector< std::vector< Uuid > > >{};

                for( auto const& lin : mlineages )
                {
                    BC_ASSERT( !lin.empty() );
                    auto const root = lin.front();

                    if( auto it = root_groups.find( root )
                      ; it != root_groups.end() )
                    {
                        it->second.emplace_back( lin );
                    }
                    else
                    {
                        root_groups.emplace( std::pair{ root, std::vector< std::vector< Uuid > >{ lin } } );
                    }
                }

                for( auto const& group : root_groups )
                {
                    auto tmap = KTRY( disambiguate_paths2( km, group.second ) );

                    rmap.insert( tmap.begin(), tmap.end() );
                }
            }
        }
    }

    rv = rmap;

    return rv;
}

SCENARIO( "disambiguate_path" , "[path]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "network" );

    auto& km = kmap::Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const decide_unique_path = [ & ]( std::string const& path )
    {
        auto const rs = REQUIRE_TRY( decide_path( km, nw->root_node(), nw->selected_node(), path ) );

        REQUIRE( rs.size() == 1 );

        return rs.back();
    };
    auto const check = [ & ]( auto const& ipath, std::vector< std::string > const& opaths ) -> bool
    {
        KM_RESULT_PROLOG();
        auto const target = decide_unique_path( ipath );
        auto const expected = [ & ]
        {
            auto es = std::set< Uuid >{};
            for( auto const& op : opaths )
            {
                auto const n =  decide_unique_path( op );
                es.emplace( n );
            }
            return es;
        }();
        auto const dis = KTRYE( disambiguate_path( km, target ) );
        auto const roots = dis
                         | rvs::values
                         | ranges::to< std::set >();
        
        return roots == expected;
    };

    GIVEN( "/task" )
    {
        REQUIRE_TRY( anchor::abs_root
                   | view2::direct_desc( "task" )
                   | act2::create_node( km ) );
        
        REQUIRE( check( "/task", { "/" } ) );

        GIVEN( "/misc.task" )
        {
            REQUIRE_TRY( anchor::abs_root
                       | view2::direct_desc( "misc.task" )
                       | act2::create_node( km ) );

            REQUIRE( check( "/task", { "/", "/misc" } ) );
            REQUIRE( check( "/misc", { "/" } ) );
            REQUIRE( check( "/misc.task", { "/", "/misc" } ) );

            GIVEN( "/misc.daily.task" )
            {
                REQUIRE_TRY( anchor::abs_root
                           | view2::direct_desc( "misc.daily.task" )
                           | act2::create_node( km ) );

                REQUIRE( check( "/task", { "/", "/misc", "/misc.daily" } ) );
                REQUIRE( check( "/misc", { "/" } ) );
                REQUIRE( check( "/misc.task", { "/", "/misc", "/misc.daily" } ) );
                REQUIRE( check( "/misc.daily", { "/" } ) );
                REQUIRE( check( "/misc.daily.task", { "/", "/misc", "/misc.daily" } ) );

                GIVEN( "/log.daily.task" )
                {
                    REQUIRE_TRY( anchor::abs_root
                               | view2::direct_desc( "log.daily.task" )
                               | act2::create_node( km ) );

                    REQUIRE( check( "/task", { "/", "/misc", "/misc.daily", "/log" } ) );
                    REQUIRE( check( "/misc", { "/" } ) );
                    REQUIRE( check( "/misc.task", { "/", "/misc", "/misc.daily", "/log" } ) );
                    REQUIRE( check( "/misc.daily", { "/misc", "/log" } ) );
                    REQUIRE( check( "/misc.daily.task", { "/", "/misc", "/misc.daily", "/log" } ) );
                    REQUIRE( check( "/log", { "/" } ) );
                    REQUIRE( check( "/log.daily", { "/log", "/misc" } ) );
                    REQUIRE( check( "/log.daily.task", { "/", "/misc", "/misc.daily", "/log" } ) );

                    GIVEN( "/dst.task[alias: misc.task]" )
                    {
                        REQUIRE_TRY( anchor::abs_root | view2::child( "dst" ) | act2::create_node( km ) );
                        auto const dst = REQUIRE_TRY( anchor::abs_root | view2::direct_desc( "dst" ) | act2::fetch_node( km ) );
                        auto const task = REQUIRE_TRY( anchor::abs_root | view2::direct_desc( "task" ) | act2::fetch_node( km ) );

                        REQUIRE_TRY( nw->create_alias( task, dst ) );

                        // TODO: A node can either be an alias src or dst. Each of these I can get from the alias_store. So, for each input, I just need to expand
                        //       to include either dsts or src. And include these in the total for "node".
                        REQUIRE( check( "/task", { "/", "/misc", "/misc.daily", "/log", "/dst" } ) );
                        REQUIRE( check( "/misc", { "/" } ) );
                        REQUIRE( check( "/misc.task", { "/", "/misc", "/misc.daily", "/log", "/dst" } ) );
                        REQUIRE( check( "/misc.daily", { "/misc", "/log" } ) );
                        REQUIRE( check( "/misc.daily.task", { "/", "/misc", "/misc.daily", "/log", "/dst" } ) );
                        REQUIRE( check( "/log", { "/" } ) );
                        REQUIRE( check( "/log.daily", { "/log", "/misc" } ) );
                        REQUIRE( check( "/log.daily.task", { "/", "/misc", "/misc.daily", "/log", "/dst" } ) );
                    }
                }
            }
        }
    }
}
 
auto complete_selection( Kmap const& kmap
                       , Uuid const& root
                       , Uuid const& selected
                       , std::string const& path )
    -> Result< CompletionNodeSet >
{
    using Set = CompletionNodeSet;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );
        KM_RESULT_PUSH_STR( "path", path );

    auto rv = KMAP_MAKE_RESULT( Set );
    auto const completed_ms = KMAP_TRY( complete_path_reducing( kmap
                                                              , root
                                                              , selected
                                                              , path ) );
    // auto const disambiguated = KMAP_TRY( disambiguate_paths( kmap, completed_ms ) );

    // rv = disambiguated;
    rv = completed_ms;

    return rv;
}

auto is_ancestor( com::Network const& nw
                , Uuid const& ancestor
                , Uuid const& descendant )
    -> bool
{
    return ancestor != descendant
        && is_lineal( nw, ancestor, descendant );
}

auto is_leaf( Kmap const& km
            , Uuid const& node )
    -> bool
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    return nw->fetch_children( node ).empty();
}

auto is_lineal( com::Network const& nw
              , Uuid const& ancestor
              , Uuid const& descendant )
    -> bool
{
    auto child = descendant;
    auto parent = nw.fetch_parent( child );

    while( parent )
    {
        if( child == ancestor )
        {
            return true;
        }
        else
        {
            child = parent.value();
            parent = nw.fetch_parent( child );
        }
    }

    if( child == ancestor ) // Account for .root being ancestor.
    {
        return true;
    }
    else
    {
        return false;
    }
}

auto is_lineal( com::Network const& nw
              , Uuid const& ancestor
              , Heading const& descendant )
    -> bool
{
    KM_RESULT_PROLOG();

    for( auto const& c : nw.fetch_children( ancestor ) )
    {
        if( KTRYE( nw.fetch_heading( c ) ) == descendant )
        {
            return true;
        }
        else if( is_lineal( nw
                          , c 
                          , descendant ) )
        {
            return true;
        }
    }

    return false;
}

// TODO: If Lineage becomes its own type, it can enforce that its composing nodes are always either ascending or descending.
//       This would mean that the Lineage argument would only be useful as UuidVec, otherwise it would be redundant.
auto is_ascending( Kmap const& kmap
                 , UuidVec const& lineage )
    -> bool
{
    KM_RESULT_PROLOG();

    auto rv = true;
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( !lineage.empty() )
    {
        auto parent = nw->fetch_parent( lineage.front() );

        for( auto const& node : lineage
                              | views::drop( 1 ) )
        {
            if( parent
             && node == parent.value() )
            {
                parent = nw->fetch_parent( parent.value() );
            }
            else
            {
                rv = false;

                break;
            }
            
        }
    }

    return rv;
}

auto is_descending( Kmap const& kmap
                  , UuidVec const& lineage )
    -> bool
{
    return is_ascending( kmap
                       , lineage | views::reverse | to< UuidVec >() );
}

auto is_parent( com::Network const& nw
              , Uuid const& parent
              , Uuid const& child )
    -> bool
{
    return KTRYB( nw.fetch_parent( child ) ) == parent;
}

auto is_sibling_adjacent( Kmap const& km
                        , Uuid const& node
                        , Uuid const& other )
    -> bool
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );
        KM_RESULT_PUSH( "other", other );
    // Ensure nodes are siblings.
    {
        auto const nw = KTRYE( km.fetch_component< com::Network >() );
        auto const np = KTRYB( nw->fetch_parent( node ) );
        auto const op = KTRYB( nw->fetch_parent( other ) );

        KENSURE_B( np == op );
    }
    
    auto const siblings = anchor::node( node )
                        | view2::sibling_incl
                        | view2::order
                        | act2::to_node_vec( km );
    auto const it1 = ranges::find( siblings, node );
    auto const it2 = ranges::find( siblings, other );

    if( it1 < it2 )
    {
        return std::distance( it1, it2 ) == 1;
    }
    else
    {
        return std::distance( it2, it1 ) == 1; 
    }
}


// Inclusive of "leaf". Exclusive of "root".
// Note: Possible to further generalize as an ancestor possessing a geometry more than one level deep by making it a RegexVector.
auto fetch_nearest_ancestor( Kmap const& kmap
                           , Uuid const& root
                           , Uuid const& leaf
                           , std::regex const& geometry )
    -> Result< Uuid > 
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "leaf", leaf );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // TODO: How many of these should be KMAP_ENSURE?
            BC_ASSERT( nw->exists( root ) );
            BC_ASSERT( nw->exists( leaf ) );
            BC_ASSERT( nw->is_lineal( root, leaf ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( rv.value() != root );
                BC_ASSERT( nw->is_lineal( root, rv.value() ) );
            }
        })
    ;

    auto child = leaf;
    auto parent = nw->fetch_parent( child );

    while( parent 
        && child != root )
    {
        if( has_geometry( kmap
                        , child
                        , geometry ) )
        {
            rv = child;
            break;
        }
        else
        {
            child = parent.value();
            parent = nw->fetch_parent( parent.value() );
        }
    }

    if( rv
     && rv.value() == root )
    {
        rv = KMAP_MAKE_RESULT( Uuid );
    }

    return rv;
}

auto fetch_nearest_ascending( Kmap const& kmap
                            , Uuid const& root
                            , Uuid const& leaf 
                            , std::function< bool( Kmap const&, Uuid const& ) > pred )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "leaf", leaf );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( nw->is_lineal( root, rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( nw->exists( root ), error_code::network::invalid_node );
    KMAP_ENSURE( nw->exists( leaf ), error_code::network::invalid_node );
    KMAP_ENSURE( nw->is_lineal( root, leaf ), error_code::network::invalid_lineage );

    auto const check = [ & ]( Uuid const& c ){ return pred( kmap, c ); }; // TODO: Replace with std::bind_front. TODO: Assume predicate has captured kmap, and just call pred( c ).
    auto child = leaf;
    auto parent = anchor::node( root )
                | view2::child( child )
                | act2::fetch_node( kmap );

    while( parent
        && !check( child ) )
    {
        child = parent.value();
        parent = anchor::node( root )
               | view2::child( child )
               | act2::fetch_node( kmap );
    }

    if( check( child ) )
    {
        rv = child;
    }

    return rv;
}

auto is_absolute( Heading const& heading )
    -> bool
{
    return !heading.empty() && ( heading[ 0 ] == '.' || heading[ 0 ] == '/' );
}

/// Returns only the newly created nodes, in order.
auto create_descendants( Kmap& kmap
                       , Uuid const& root
                       , Uuid const& selected
                       , Heading const& heading )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_POST( [ & ] 
        {
            if( rv )
            {
                BC_ASSERT( !rv.has_error() );
                // BC_ASSERT( ( !( rv.value().empty() ) ) ); // TODO: Very strange bug. Enable at own sanity's risk. Checking post-return inside dtor here gives different answer to empty() question than right before return statement and returned value to caller.
            }
        } )
    ;

    KMAP_ENSURE( nw->exists( root ), error_code::network::invalid_root );
    KMAP_ENSURE( nw->is_lineal( root, selected ), error_code::network::invalid_lineage );
    
    // Algorithm:
    // 1. Tokenize
    // 1. For each heading token, going forward, attempt to decide the path. If it resolves, continue. 
    // 1. If not, create child for the heading.
    auto const all_tokens = tokenize_path( heading );
    auto walking_tokens = decltype( all_tokens ){};
    auto all_tokens_it = all_tokens.begin();
    auto const all_tokens_eit = all_tokens.end();
    auto new_additions = UuidVec{};

    BC_ASSERT( !rv );
    BC_ASSERT( new_additions.empty() );

    while( all_tokens_it != all_tokens_eit )
    {
        auto const curr_token = *all_tokens_it;
        ++all_tokens_it;
        walking_tokens.emplace_back( curr_token );

        auto const decider = walk( kmap
                                 , root
                                 , selected
                                 , walking_tokens );
        auto const curr_path = walking_tokens | views::join | to< std::string >();
        
        // Ok, so here's the problem. I need a delayed state. I can't create a child based on the prospects.back() if the prospect disappears (is "laundered")
        // as soon as its found to be in error.
        //    (1) You must communicate "full" errors from UniqueDecider to Decider on the same event.
        //    (2) Launder should occur as a pre, not post, so errors are preserved, for at least one cycle.
        if( decider.driver->is( boost::sml::state< sm::state::Error > ) )
        {
            if( !is_valid_heading_path( curr_token ) )
            {
                rv = KMAP_MAKE_ERROR_MSG( error_code::network::invalid_heading, fmt::format( "'{}' in '{}'", curr_token, heading ) );

                break;
            }
            else if( decider.output->prospects.size() > 1 )
            {
                auto const to_abs_path = [ & ]( auto const& prospect )
                {
                    KMAP_ENSURE_EXCEPT( prospect.output != nullptr );
                    KMAP_ENSURE_EXCEPT( prospect.output->prospect.size() != 0 );

                    return KTRYE( anchor::abs_root
                                | view2::desc( prospect.output->prospect.back() )
                                | act2::abs_path_flat( kmap ) );
                };

                auto const prospects_flattened = decider.output->prospects
                                               | ranges::views::transform( to_abs_path )
                                               | ranges::views::join( ',' )
                                               | ranges::to< std::string >();

                rv = KMAP_MAKE_ERROR_MSG( error_code::network::ambiguous_path, fmt::format( "{} ambiguous with: {}\n", heading, prospects_flattened ) );

                break;
            }
            else if( decider.output->prospects.size() == 0 )
            {
                rv = KMAP_MAKE_ERROR_MSG( error_code::network::invalid_heading, heading );
                
                break;
            }
            else
            {
                BC_ASSERT( decider.output->prospects.size() == 1 );

                KMAP_ENSURE_EXCEPT( is_valid_heading( curr_token ) );
                KMAP_ENSURE_EXCEPT( decider.output->prospects.back().output != nullptr );
                KMAP_ENSURE_EXCEPT( decider.output->prospects.back().output->prospect.size() != 0 );

                auto const ccr = nw->create_child( decider.output->prospects.back().output->prospect.back(), curr_token );

                if( ccr )
                {
                    // auto pros = output->prospects.front().second->prospect;
                    // pros.emplace_back( ccr.value() );
                    // new_additions.insert( new_additions.end()
                    //                     , pros.begin(), pros.end() );
                    new_additions.emplace_back( ccr.value() );
                    BC_ASSERT( !new_additions.empty() );

                    rv = new_additions;
                    BC_ASSERT( !rv.value().empty() );
                }
                else
                {
                    rv = KMAP_PROPAGATE_FAILURE( ccr );
                    break;
                }
            }
        }
        else
        {
            rv = KMAP_MAKE_ERROR_MSG( error_code::network::child_already_exists, heading );
            // TODO: Should something be returned here?
            //       The description of this function says it only returns newly created IDs.
            //       In this case, the success case, no new node is created.
            // BC_ASSERT( !output->prospects.empty() );
        }
    }

    return rv;
}

auto fetch_descendant( Kmap const& kmap
                     , Uuid const& root // TODO: Lineal const& root_selected
                     , Uuid const& selected
                     , Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( rv.value() != root );
            }
        })
    ;

    auto const decided = KMAP_TRY( decide_path( kmap
                                              , root
                                              , selected
                                              , heading ) );
    auto const desc = decided
                    | views::filter( [ &root ]( auto const& e ){ return root != e; } )
                    | ranges::to< UuidVec >();

    if( desc.size() == 1 )
    {
        rv = desc.back();
    }
    else
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::network::ambiguous_path, heading );
    }
    
    return rv;
}

// TODO: There is some ambiguity here, as descendant could be anywhere down the descent lines. Would it not be more precise to say lineal/direct_descendant rather than descendant?
auto fetch_or_create_descendant( Kmap& kmap
                               , Uuid const& root
                               , Uuid const& selected
                               , Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );
        KM_RESULT_PUSH_NODE( "selected", selected );
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    if( auto const desc = fetch_descendant( kmap, root, selected, heading )
      ; desc )
    {
        rv = desc;
    }
    else
    {
        KTRY( create_descendants( kmap
                                , root
                                , selected
                                , heading ) );
        
        rv = KTRY( fetch_descendant( kmap
                                   , root
                                   , selected
                                   , heading ) );
    }

    return rv;
}

/**
 * Performs only a basic "mirroring".
 * Mirror: Ensures `dst` has similar nodes to `src`.
 * Basic: Node name and title. All other node attributes are ignored.
 * Allows for existent lineages. Extends existent lineages.
 * Return: last node mirrored.
 * 
 * TODO: Would not this be better to rename to "mirror" and take a fn( kmap, src, dst ) -> Result< void >
 *       that does any of the copying required? If you want to fail if, e.g., a heading matches, 
 *       but the title does not, then the function can return an error Result< void >, halting the mirroring.
 *       Example: mirror( kmap, src, dst, attr::heading | attr::title );
 *       Example: mirror( kmap, src, dst, attr::all );
 *       Example: mirror( kmap, src, dst, []( auto const& lhs, auto const& rhs ){ ...; } );
 * 
 *       If variadic as the last param, it could call an arbitrary number of functions.
 *       This would mean attr::heading, etc. would actually be small functions that update( id ).
 **/
auto mirror_basic( Kmap& kmap
                 , LinealRange const& src // TODO: This should be UuidVec, not LinealRange. Why? There's insufficient reason to constrain the nodes to mirror to an existent lineage. Why not a combination of nonlineal nodes? That would make it more generalized.
                 , Uuid const& dst )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "dst", dst );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto dn = dst;

    if( ranges::distance( src ) == 0 )
    {
        rv = dst;
    }
    else
    {
        for( auto const& sn : src )
        {
            auto const heading = nw->fetch_heading( sn ).value();
            auto const title = KTRY( nw->fetch_title( sn ) );
            auto const c = nw->create_child( dn, heading, title );

            if( !c )
            {
                if( c.error().ec == error_code::create_node::duplicate_child_heading )
                {
                    dn = KTRY( nw->fetch_child( dn, heading ) );
                    rv = dn;
                }
                else
                {
                    rv = c;

                    break;
                }
            }
            else
            {
                dn = c.value();
                rv = dn;
            }
        }
    }

    return rv;
}

#if 0
Heading::Heading( std::string_view const path )
{
    if( !is_valid_heading( path ) )
    {
        // TODO: throw exception....
        assert( false );
    }

    str_ = path;
}

Heading::Heading( char const* heading )
    : Heading{ heading
             , strlen( heading ) }
{
}

auto Heading::begin()
    -> std::string::iterator
{
    return str_.begin();
};

auto Heading::cbegin() const
    -> std::string::const_iterator
{
    return str_.cbegin();
};

auto Heading::end()
    -> std::string::iterator
{
    return str_.end();
}

auto Heading::cend() const
    -> std::string::const_iterator
{
    return str_.end();
}

HeadingPath::HeadingPath( std::string_view const path )
{
    if( !is_valid_heading( path ) )
    {
        // TODO: throw exception....
        assert( false );
    }

    headings_.emplace_back( path );
    it_ = headings.begin();
}

HeadingPath::HeadingPath( Heading const& heading )
    : headings_{ heading }
{
}

auto HeadingPath::begin()
    -> std::string::iterator
{
    return headings_.begin();
}

auto HeadingPath::begin() const
    -> std::string::const_iterator
{
    return headings_.cbegin();
}

auto HeadingPath::cbegin() const
    -> std::string::const_iterator
{
    return headings_.cbegin();
}

auto HeadingPath::end()
    -> std::string::iterator
{
    return headings_.end();
}

auto HeadingPath::end() const
    -> std::string::const_iterator
{
    return headings_.cend();
}

auto HeadingPath::cend() const
    -> std::string::const_iterator
{
    return headings_.cend();
}

auto operator/( HeadingPath const& lhs
              , HeadingPath const& rhs )
    -> HeadingPath 
{
    return { lhs
           , rhs }
         | views::join;
}
#endif // 0

} // namespace kmap
