/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path.hpp"

#include "common.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "error/network.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "lineage.hpp"
#include "network.hpp"
#include "path/sm.hpp"
#include "utility.hpp"

#include <boost/sml.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
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
            auto const& db = kmap_.database();

            return db.heading_exists( ev );
        };

        auto is_root = [ & ]( auto const& ev )
        {
            return kmap_.fetch_heading( root_id_ ).value() == ev; // TODO: Does this fail when another node is named "root"?
        };

        auto is_selected_root = [ & ]( auto const& ev )
        {
            return selected_node_ == root_id_;
        };

        auto is_child = [ & ]( auto const& ev )
        {
            auto fn = [ & ]( auto const& e )
            {
                if( e.empty() )
                {
                    return false;
                }

                auto const parent = e.back();
                auto const child = kmap_.fetch_child( parent
                                                    , ev );

                return bool{ child };
            };
            auto const it = find_if( paths()
                                   , fn );
        
            return it != end( paths() );
        };

        auto has_parent = [ & ]( auto const& ev )
        {
            auto fn = [ & ]( auto const& e )
            {
                BC_ASSERT( !e.empty() );

                auto const current = e.back();

                return kmap_.fetch_parent( current ).has_value();
            };
            auto const it = find_if( paths()
                                   , fn );
        
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

        //         return bool{ kmap_.fetch_parent( e.back() ) };
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
            auto filter = views::filter( [ & ]( auto const& e )
            {
                auto const heading = kmap_.fetch_heading( e );
                BC_ASSERT( heading );
                return ev == heading.value();
            } );
            auto const heading = ev;
            auto const all_ids = kmap_.fetch_nodes( heading );

            paths() = all_ids
                    | filter
                    | views::transform( [ & ]( auto&& e ){ return UuidPath{ e }; } )
                    | to< UuidPaths >();
        };

        auto push_headings = [ & ]( auto const& ev )
        {
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

            for( auto&& path : paths() )
            {
                if( auto const id = kmap_.fetch_child( path.back()
                                                     , ev )
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
            for( auto&& path : paths() )
            {
                BC_ASSERT( !path.empty() ); // Other pertinent actions should ensure empty paths are removed.

                io::print( "pushing parent of: {}\n", kmap_.fetch_heading( path.back() ).value() );

                auto const node = path.back();

                if( auto const parent = kmap_.fetch_parent( node )
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
            io::print( "pushing parent of selected: {}\n", kmap_.fetch_heading( selected_node_ ).value() );

            auto const parent = kmap_.fetch_parent( selected_node_ );

            BC_ASSERT( parent );

            paths().emplace_back( UuidPath{ parent.value() } );
        };

        auto push_all_children = [ & ]( auto const& ev )
        {
            auto new_paths = UuidPaths{};

            for( auto const& path : paths() )
            {
                // TODO: assert: !path.empty()
                for( auto const id : kmap_.fetch_children( path.back() ) )
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
            auto new_paths = UuidPaths{};

            for( auto const& path : paths() )
            {
                BC_ASSERT( !path.empty() );

                if( auto const parent = kmap_.fetch_parent( path.back() )
                  ; parent )
                {
                    BC_ASSERT( !parent );

                    for( auto const id : kmap_.fetch_children( parent.value() ) )
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
            // BC_CONTRACT()
            //     BC_POST([ & ]
            //     {
            //         // BC_ASSERT( !possible_paths_.empty() );
            //     })
            // ;
            
            auto const known = kmap_.fetch_heading( root_id_ ).value();
            auto const ml = match_length( ev
                                        , known );

            if( ml == ev.size() )
            {
                paths().emplace_back( UuidPath{ root_id_ } );
            }
        };

        auto complete_any = [ & ]( auto const& ev )
        {
            // BC_CONTRACT()
            //     BC_POST([ & ]
            //     {
            //         // BC_ASSERT( !possible_paths_.empty() );
            //     })
            // ;

            auto const& db = kmap_.database();
            auto const& headings = db.fetch_headings().get< 2 >();
            auto matches = std::vector< UuidPath >{};
            
            for( auto it = headings.lower_bound( ev )
               ; it != headings.end()
               ; ++it )
            {
                if( it->second.compare( 0, ev.size(), ev ) != 0 )
                {
                    break;
                }

                auto const node_and_aliases = [ & ]
                {
                    auto all = kmap_.fetch_aliases_to( it->first );

                    all.emplace_back( it->first );

                    return all;
                }();

                for( auto const& e : node_and_aliases )
                {
                    if( kmap_.is_lineal( root_id_, Uuid{ e } ) )
                    {
                        matches.emplace_back( UuidPath{ e } );
                    }
                }
            }

            paths() = matches;
        };

        auto complete_path = [ & ]( auto const& ev )
        {
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

            auto new_paths = [ & ]
            {
                auto rv = std::vector< UuidPath >{};

                for( auto const& path : paths() )
                {
                    auto const cids = kmap_.fetch_children( path.back() );

                    for( auto const cid : cids )
                    {
                        if( ev.size() == match_length( ev
                                                     , kmap_.fetch_heading( kmap_.resolve( cid ) ).value() ) )
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
    auto rv = StringVec{};
    auto const rgx = std::regex{ R"([\.\,\/\']|[^\.\,\/\']+)" }; // .,/' or one or more of none of these: heading.

    rv = raw
       | views::tokenize( rgx )
       | to< StringVec >();

    return rv;
}

auto complete_any( Kmap const& kmap
                 , Uuid const& root
                 , std::string const& heading )
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );

    KMAP_ENSURE( rv, is_valid_heading( heading ), error_code::network::invalid_heading );

    auto const& db = kmap.database();
    auto const& headings = db.fetch_headings().get< 2 >();
    auto matches = UuidSet{};
    
    for( auto it = headings.lower_bound( heading )
       ; it != headings.end()
       ; ++it )
    {
        if( it->second.compare( 0, heading.size(), heading ) != 0 )
        {
            break;
        }

        auto const node_and_aliases = [ & ]
        {
            auto all = kmap.fetch_aliases_to( it->first );

            all.emplace_back( it->first );

            return all;
        }();

        for( auto const& e : node_and_aliases )
        {
            if( kmap.is_lineal( root, e ) )
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

    KMAP_ENSURE( rv, kmap.is_lineal( root, selected ), error_code::network::invalid_lineage );

    auto const tokens = tokenize_path( raw );

    if( !tokens.empty() )
    {
        auto comps = Set{};

        if( is_valid_heading( tokens.back() ) ) // ends in heading
        {
            if( tokens.size() == 1 )
            {
                auto const nodes = KMAP_TRY( complete_any( kmap, root, raw ) );
                comps = nodes
                      | views::transform( [ & ]( auto const& e ){ return CompletionNode{ .target=e, .path=kmap.fetch_heading( e ).value() }; } )
                      | to< Set >();
            }
            else
            {
                auto [ decider, output ] = walk( kmap, root, selected, tokens | views::drop_last( 1 ) | to< StringVec >() );

                for( auto&& prospect : output->prospects )
                {
                    if( prospect.first->is( boost::sml::state< sm::state::FwdNode > ) )
                    {
                        for( auto const& child : kmap.fetch_children( prospect.second->prospect.back() ) )
                        {
                            auto const ch = kmap.fetch_heading( child ).value();

                            if( ch.starts_with( tokens.back() ) )
                            {
                                auto const joined = tokens | views::drop_last( 1 ) | views::join | to< std::string >();
                                auto const np = joined + ch;
                                comps.emplace( CompletionNode{ .target=child, .path=np } );
                            }
                        }
                    }
                    else if( prospect.first->is( boost::sml::state< sm::state::BwdNode > ) )
                    {
                        BC_ASSERT( !prospect.second->prospect.empty() );

                        auto const p = prospect.second->prospect.back();
                        auto const ph = kmap.fetch_heading( p ); BC_ASSERT( ph );

                        if( ph.value().starts_with( tokens.back() ) )
                        {
                            auto const joined = tokens | views::drop_last( 1 ) | views::join | to< std::string >();
                            auto const np = joined + ph.value();
                            comps.emplace( CompletionNode{ .target=p, .path=np } );
                        }
                    }
                    else if( prospect.first->is( boost::sml::state< sm::state::DisNode > ) )
                    {
                        BC_ASSERT( !prospect.second->disambiguation.empty() );

                        auto const p = kmap.fetch_parent( prospect.second->disambiguation.back() ); BC_ASSERT( p );
                        auto const ph = kmap.fetch_heading( p.value() ); BC_ASSERT( ph );

                        if( ph.value().starts_with( tokens.back() ) )
                        {
                            auto const joined = tokens | views::drop_last( 1 ) | views::join | to< std::string >();
                            auto const np = joined + ph.value();
                            auto const target = prospect.second->prospect.back();
                            BC_ASSERT( !prospect.second->disambiguation.empty() );
                            auto const disp = kmap.fetch_parent( prospect.second->disambiguation.back() ); BC_ASSERT( disp );
                            comps.emplace( CompletionNode{ .target=target, .path=np, .disambig={ disp.value() } } );
                        }
                    }
                }
            }

            // Append append ,.
            auto tails = Set{};
            for( auto const& comp : comps )
            {
                if( kmap.fetch_heading( comp.target ).value() == tokens.back() )
                {
                    auto const joined = tokens | views::join | to< std::string >();

                    if( auto const parent = kmap.fetch_parent( comp.target )
                      ; parent && comp.target != root )
                    {
                        tails.emplace( CompletionNode{ .target=parent.value(), .path=fmt::format( "{},", joined ) } );
                    }

                    if( !kmap.fetch_children( comp.target ).empty() )
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
                               return tokens.back() == kmap.fetch_heading( target ).value();
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
                               auto const tdisp = kmap.fetch_parent( tdis ); BC_ASSERT( tdisp );
                               auto ndis = e.disambig;
                               ndis.emplace_back( tdisp.value() );
                               return CompletionNode{ .target=e.target, .path=io::format( "{}'{}", e.path, kmap.fetch_heading( tdisp.value() ).value() ), .disambig=ndis };
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
            auto [ decider, output ] = walk( kmap, root, selected, tokens );

            for( auto&& prospect : output->prospects )
            {
                if( prospect.first->is( boost::sml::state< sm::state::FwdNode > ) )
                {
                    for( auto const& child : kmap.fetch_children( prospect.second->prospect.back() ) )
                    {
                        auto const joined = tokens | views::join | to< std::string >();
                        auto const np = joined + kmap.fetch_heading( child ).value();

                        comps.emplace( CompletionNode{ .target=child, .path=np } );
                    }
                }
                else if( prospect.first->is( boost::sml::state< sm::state::BwdNode > ) )
                {
                    auto const target = prospect.second->prospect.back();
                    auto const joined = tokens | views::join | to< std::string >();
                    auto const np = joined + kmap.fetch_heading( target ).value();

                    comps.emplace( CompletionNode{ .target=target, .path=np } );
                }
                else if( prospect.first->is( boost::sml::state< sm::state::DisNode > ) )
                {
                    auto const target = prospect.second->disambiguation.back();
                    auto const p = kmap.fetch_parent( target); BC_ASSERT( p );
                    auto const joined = tokens | views::join | to< std::string >();
                    auto const np = joined + kmap.fetch_heading( p.value() ).value();

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
    auto rv = IdHeadingSet{};
    auto const children = kmap.fetch_children( parent );

    for( auto const& cid : children )
    {
        auto const cheading = kmap.fetch_heading( cid ).value();
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

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const [ sm, output ] = walk( kmap
                                    , root
                                    , selected
                                    , tokens );

    if( sm->is( boost::sml::state< sm::state::Error > ) )
    {
        auto const joined = tokens | views::join | to< std::string >();
        rv = KMAP_MAKE_ERROR_MSG( error_code::node::invalid_path, io::format( "\n\terror: {}\n\theading: {}", output->error_msg, joined ) );
    }
    else
    {
        rv = output->prospects
           | views::filter( []( auto const& e ){ return !e.first->is( boost::sml::state< sm::state::Error > ); } ) // Note: b/c the SM allows a "delay" - perserving the failure for at least one cycle, must filter out manually.
           | views::transform( []( auto const& e ){ BC_ASSERT( e.second && !e.second->prospect.empty() ); return e.second->prospect.back(); } )
           | to< UuidVec >();
    }

    return rv;
}

/**
 * Returns all matches (if more than one => ambiguous) that could be resolved from the path given by "raw".
 **/
auto decide_path( Kmap const& kmap
                , Uuid const& root
                , Uuid const& selected
                , std::string const& raw )
    -> Result< UuidVec >
{

    auto rv = KMAP_MAKE_RESULT( UuidVec );

    KMAP_ENSURE( rv, !raw.empty(), error_code::node::invalid_heading );
    KMAP_ENSURE_MSG( rv, is_valid_heading_path( raw ), error_code::node::invalid_heading, raw );
    KMAP_ENSURE_MSG( rv, kmap.is_lineal( root, selected ), error_code::node::not_lineal, io::format( "root `{}` not lineal to selected `{}`\n", kmap.absolute_path_flat( root ), kmap.absolute_path_flat( selected ) ) );

    auto const tokens = tokenize_path( raw );

    rv = decide_path( kmap
                    , root
                    , selected
                    , tokens );

    return rv;
}

// TODO: This seems to be a misnomer, or, at least, a misleading name. The "descendants" refers to "root_id" given,
//       but such a convention can be easily understood to be the descendants of the path instead.
auto fetch_descendants( Kmap const& kmap
                      , Uuid const& root_id
                      , Uuid const& selected_node
                      , std::string const& raw )
    -> Result< UuidVec >
{
    return decide_path( kmap
                      , root_id
                      , selected_node
                      , raw );
}

auto has_geometry( Kmap const& kmap
                 , Uuid const& parent 
                 , std::regex const& geometry )
    -> bool
{
    for( auto const& child : kmap.fetch_children( parent ) )
    {
        auto const heading = kmap.fetch_heading( child );
        assert( heading );
        auto result = std::smatch{};

        if( std::regex_match( heading.value()
                            , result
                            , geometry ) )
        {
            return true;
        }
    }

    return false;
}

auto fetch_direct_descendants( Kmap const& kmap
                             , Uuid const& root
                             , Heading const& descendant_geometry )
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( root ) );
        })
        BC_POST([ & ]
        {
            for( auto const& id : rv )
            {
                BC_ASSERT( kmap.exists( id ) );
            }
        })
    ;

    for( auto const& child : kmap.fetch_children( root ) )
    {
        auto const matches = fetch_direct_descendants( kmap
                                                     , child 
                                                     , descendant_geometry );
        
        rv.insert( rv.begin()
                 , matches.begin()
                 , matches.end() );

        auto const match = kmap.fetch_leaf( child
                                          , child
                                          , descendant_geometry );
        
        if( match )
        {
            rv.emplace_back( child );
        }
    }

    return rv;
}

// TODO: Shouldn't this limit the root to a range, not absolute root? That is, say a path is completed (subroot, heading), then fed here.
//       This makes no account of such a subroot, only of the absolute root.
auto disambiguate_paths( Kmap const& kmap
                       , CompletionNodeSet const& node_set )
    -> Result< std::map< Uuid, std::string > >
{
    using ResMap = std::map< Uuid, std::string >;

    struct Node
    {
        std::string path = {};
        Uuid disambig = {};
        CompletionNode comp = {};
    };

    auto rv = KMAP_MAKE_RESULT( ResMap );
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
            KMAP_ENSURE( rv, kmap.root_node_id() != dup.disambig, error_code::network::invalid_node );
        }
        auto const expanded = duplicates
                            | views::transform( [ & ]( auto const& e )
                            {
                                     io::print( "disambiguating: {}\n", e.comp.path );
                                 if( e.path.ends_with( '.' ) ) 
                                 {
                                    auto const p = kmap.fetch_parent( e.disambig ).value();
                                    auto const ph = kmap.fetch_heading( p ).value();

                                    return Node{ io::format( "{}'{}."
                                                           , e.path | views::drop_last( 1 ) | to< std::string >() // TODO: drop_last_while( '.' ) to remove all ending '.' if multiple?
                                                           , ph )
                                               , p
                                               , e.comp };
                                 }
                                 else if( e.path.ends_with( ',' ) )
                                 {
                                    auto const p = kmap.fetch_parent( e.disambig ).value(); // TODO: Once input is in the form of UuidPath, use *( target.end() - 2 ) to get parent.
                                    auto const ph = kmap.fetch_heading( p ).value();

                                    return Node{ io::format( "{}'{},"
                                                           , e.path | views::drop_last( 1 ) | to< std::string >() // TODO: drop_last_while( '.' ) to remove all ending '.' if multiple?
                                                           , ph )
                                               , p
                                               , e.comp };
                                 }
                                 else if( e.path.ends_with( '\'' ) )
                                 {
                                     io::print( "disambiguating ending in': {}\n", e.path );
                                    auto const p = kmap.fetch_parent( e.disambig ).value();
                                    auto const ph = kmap.fetch_heading( p ).value();

                                    return Node{ io::format( "{}{}"
                                                           , e.path
                                                           , ph )
                                               , p
                                               , e.comp };
                                 }
                                 else
                                 {
                                    auto const p = kmap.fetch_parent( e.disambig ).value();
                                    auto const ph = kmap.fetch_heading( p ).value();

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

auto complete_selection( Kmap const& kmap
                       , Uuid const& root
                       , Uuid const& selected
                       , std::string const& path )
    -> Result< CompletionNodeSet >
{
    using Set = CompletionNodeSet;

    auto rv = KMAP_MAKE_RESULT( Set );
    auto const completed_ms = KMAP_TRY( complete_path_reducing( kmap
                                                              , kmap.root_node_id()
                                                              , kmap.selected_node()
                                                              , path ) );
    // auto const disambiguated = KMAP_TRY( disambiguate_paths( kmap, completed_ms ) );

    // rv = disambiguated;
    rv = completed_ms;

    return rv;
}

auto is_ancestor( Kmap const& kmap
                , Uuid const& ancestor
                , Uuid const& descendant )
    -> bool
{
    return ancestor != descendant
        && is_lineal( kmap, ancestor, descendant );
}

auto is_lineal( Kmap const& kmap
              , Uuid const& ancestor
              , Uuid const& descendant )
    -> bool
{
    auto child = descendant;
    auto parent = kmap.fetch_parent( child );

    while( parent )
    {
        if( child == ancestor )
        {
            return true;
        }
        else
        {
            child = parent.value();
            parent = kmap.fetch_parent( child );
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

auto is_lineal( Kmap const& kmap
              , Uuid const& ancestor
              , Heading const& descendant )
    -> bool
{
    for( auto const& c : kmap.fetch_children( ancestor ) )
    {
        if( kmap.fetch_heading( c ).value() == descendant )
        {
            return true;
        }
        else if( is_lineal( kmap
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
    auto rv = true;

    if( !lineage.empty() )
    {
        auto parent = kmap.fetch_parent( lineage.front() );

        for( auto const& node : lineage
                              | views::drop( 1 ) )
        {
            if( parent
             && node == parent.value() )
            {
                parent = kmap.fetch_parent( parent.value() );
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

// Inclusive of "leaf". Exclusive of "root".
// Note: Possible to further generalize as an ancestor possessing a geometry more than one level deep by making it a RegexVector.
auto fetch_nearest_ancestor( Kmap const& kmap
                           , Uuid const& root
                           , Uuid const& leaf
                           , std::regex const& geometry )
    -> Result< Uuid > 
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // TODO: How many of these should be KMAP_ENSURE?
            BC_ASSERT( kmap.exists( root ) );
            BC_ASSERT( kmap.exists( leaf ) );
            BC_ASSERT( kmap.is_lineal( root
                                     , leaf ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( rv.value() != root );
                BC_ASSERT( kmap.is_lineal( root, rv.value() ) );
            }
        })
    ;

    auto child = leaf;
    auto parent = kmap.fetch_parent( child );

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
            parent = kmap.fetch_parent( parent.value() );
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
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( kmap.is_lineal( root, rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( rv, kmap.exists( root ), error_code::network::invalid_node );
    KMAP_ENSURE( rv, kmap.exists( leaf ), error_code::network::invalid_node );
    KMAP_ENSURE( rv, kmap.is_lineal( root, leaf ), error_code::network::invalid_lineage );

    auto const check = [ & ]( Uuid const& c ){ return pred( kmap, c ); }; // TODO: Replace with std::bind_front. TODO: Assume predicate has captured kmap, and just call pred( c ).
    auto child = leaf;
    auto parent = kmap.fetch_parent( root, child );

    while( parent
        && !check( child ) )
    {
        child = parent.value();
        parent = kmap.fetch_parent( root, child );
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
    return !heading.empty() && heading[ 0 ] == '.';
}

/// Returns only the newly created nodes, in order.
auto create_descendants( Kmap& kmap
                       , Uuid const& root
                       , Uuid const& selected
                       , Heading const& heading )
    -> Result< UuidVec >
{
    auto rv = KMAP_MAKE_RESULT( UuidVec );

    BC_CONTRACT()
        BC_POST( [ & ] 
        {
        } )
    ;

    KMAP_ENSURE( rv, kmap.exists( root ), error_code::network::invalid_root );
    KMAP_ENSURE( rv, kmap.is_lineal( root, selected ), error_code::network::invalid_lineage );
    
    // Algorithm:
    // 1. Tokenize
    // 1. For each heading token, going forward, attempt to decide the path. If it resolves, continue. 
    // 1. If not, create child for the heading.
    auto const all_tokens = tokenize_path( heading );
    auto walking_tokens = decltype( all_tokens ){};
    auto all_tokens_it = all_tokens.begin();
    auto new_additions = UuidVec{};

    while( walking_tokens.size() != all_tokens.size() )
    {
        auto const curr_token = *all_tokens_it;
        walking_tokens.emplace_back( curr_token );
        ++all_tokens_it;

        auto const [ sm, output ] = walk( kmap
                                        , root
                                        , selected
                                        , walking_tokens );
        auto const curr_path = walking_tokens | views::join | to< std::string >();
        
        // Ok, so here's the problem. I need a delayed state. I can't create a child based on the prospects.back() if the prospect disappears (is "laundered")
        // as soon as its found to be in error.
        //    (1) You must communicate "full" errors from UniqueDecider to Decider on the same event.
        //    (2) Launder should occur as a pre, not post, so errors are preserved, for at least one cycle.
        if( sm->is( boost::sml::state< sm::state::Error > ) )
        {
            if( !is_valid_heading( curr_token ) )
            {
                rv = KMAP_MAKE_ERROR_MSG( error_code::network::invalid_heading, heading );
                break;
            }
            else if( output->prospects.size() > 1 )
            {
                rv = KMAP_MAKE_ERROR_MSG( error_code::network::ambiguous_path, heading );
                break;
            }
            else if( output->prospects.size() == 0 )
            {
                rv = KMAP_MAKE_ERROR_MSG( error_code::network::invalid_heading, heading );
                break;
            }
            else
            {
                BC_ASSERT( output->prospects.size() == 1 );

                auto const ccr = kmap.create_child( output->prospects.back().second->prospect.back(), curr_token );

                if( ccr )
                {
                    auto pros = output->prospects.front().second->prospect;
                    pros.emplace_back( ccr.value() );
                    new_additions.insert( new_additions.end()
                                        , pros.begin(), pros.end() );
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
            // TODO: Should something be returned here?
            //       The description of this function says it only returns newly created IDs.
            //       In this case, the success case, no new node is created.
            // BC_ASSERT( !output->prospects.empty() );
        }

        rv = new_additions;
    }

    return rv;
}

auto fetch_descendant( Kmap const& kmap
                     , Uuid const& root // TODO: Lineal const& root_selected
                     , Uuid const& selected
                     , Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const desc = KMAP_TRY( decide_path( kmap
                                           , root
                                           , selected
                                           , heading ) );

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
    auto rv = KMAP_MAKE_RESULT( Uuid );
    if( auto const desc = fetch_descendant( kmap, root, selected, heading )
      ; desc )
    {
        rv = desc;
    }
    else
    {
        KMAP_TRY( create_descendants( kmap
                                    , root
                                    , selected
                                    , heading ) );
        
        rv = fetch_descendant( kmap
                             , root
                             , selected
                             , heading );
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
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto dn = dst;

    if( ranges::distance( src ) == 0 )
    {
        rv = dst;
    }
    else
    {
        for( auto const& sn : src )
        {
            auto const heading = kmap.fetch_heading( sn ).value();
            auto const title = kmap.fetch_title( sn ).value();
            auto const c = kmap.create_child( dn, heading, title );

            if( !c )
            {
                if( c.error().ec == error_code::create_node::duplicate_child_heading )
                {
                    dn = KMAP_TRY( kmap.fetch_child( dn, heading ) );
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
