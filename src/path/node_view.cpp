/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/node_view.hpp"

#include "com/database/db.hpp"
#include "com/network/network.hpp"
#include "com/tag/tag.hpp"
#include "common.hpp"
#include "contract.hpp"
#include "error/network.hpp"
#include "kmap.hpp"
#include "lineage.hpp"
#include "path.hpp"
#include "path/act/to_string.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/none_of.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

#include <vector>

using namespace kmap::test;

namespace kmap::view {

auto count( Kmap const& kmap )
    -> Count
{
    return Count( kmap );
}

auto create_node( Kmap& kmap )
    -> CreateNode
{
    return CreateNode{ kmap };
}

auto erase_node( Kmap& kmap )
    -> EraseNode
{
    return EraseNode{ kmap };
}

auto exists( Kmap const& kmap )
    -> Exists 
{
    return Exists{ kmap };
}

auto fetch_or_create_node( Kmap& kmap )
    -> FetchOrCreateNode
{
    return FetchOrCreateNode{ kmap };
}

auto fetch_node( Kmap const& kmap )
    -> FetchNode
{
    return FetchNode{ kmap };
}

auto to_abs_path( Kmap const& kmap )
    -> ToAbsPath
{
    return ToAbsPath{ kmap };
}

auto to_heading_set( Kmap const& kmap )
    -> ToHeadingSet 
{
    return ToHeadingSet{ kmap };
}

auto to_node_set( Kmap const& kmap )
    -> ToNodeSet 
{
    return ToNodeSet{ kmap };
}

auto operator==( SelectionVariant const& lhs
               , Heading const& rhs )
    -> bool
{
    auto rv = false;

    std::visit( [ & ]( auto const& arg ) mutable
                {
                    using T = std::decay_t< decltype( arg ) >;

                    auto matches_heading = [ &rhs ]( auto const& e ){ return e == rhs; };

                    if constexpr( std::is_same_v< T, char const* > )
                    {
                        rv = ( std::string{ arg } == rhs );
                    }
                    else if constexpr( std::is_same_v< T, std::string > )
                    {
                        rv = ( arg == rhs );
                    }
                    else if constexpr( std::is_same_v< T, all_of > )
                    {
                        // TODO: I don't think "all_of" makes sense for a set against a single heading. 
                        //       If that's true, what is appropriate behavior? Always return false?
                        rv = ranges::all_of( arg.data, matches_heading );
                    }
                    else if constexpr( std::is_same_v< T, any_of > )
                    {
                        rv = ranges::any_of( arg.data, matches_heading );
                    }
                    else if constexpr( std::is_same_v< T, none_of > )
                    {
                        rv = ranges::none_of( arg.data, matches_heading );
                    }
                    else if constexpr( std::is_same_v< T, exactly > )
                    {
                        rv = ( arg.data.size() == 1 ? ( *arg.data.begin() == rhs ) : false );
                    }
                    else if constexpr( std::is_same_v< T, Intermediary > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                    }
                    else if constexpr( std::is_same_v< T, Uuid > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                        // TODO: Missing 'kmap' here... so impl. can't be done until I account for that.
                        // if( auto const h = nw->fetch_heading( arg )
                        //   ; h )
                        // {
                        //     rv = ( h.value() == rhs );
                        // }
                    }
                    else if constexpr( std::is_same_v< T, Uuid > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                    }
                    else if constexpr( std::is_same_v< T, UuidSet > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                    }
                    else if constexpr( std::is_same_v< T, PredFn > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                    }
                    else
                    {
                        static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                    }
                }
              , lhs );

    return rv;
}

auto operator==( SelectionVariant const& lhs
               , std::set< Heading > const& rhs )
    -> bool
{
    auto rv = false;

    std::visit( [ & ]( auto const& arg ) mutable
                {
                    using T = std::decay_t< decltype( arg ) >;

                    auto const contains_heading = [ &rhs ]( auto const& e ){ return rhs.contains( e ); };

                    if constexpr( std::is_same_v< T, char const* > )
                    {
                        rv = contains_heading( arg );
                    }
                    else if constexpr( std::is_same_v< T, std::string > )
                    {
                        rv = contains_heading( arg );
                    }
                    else if constexpr( std::is_same_v< T, all_of > )
                    {
                        rv = ranges::all_of( arg.data, contains_heading );
                    }
                    else if constexpr( std::is_same_v< T, any_of > )
                    {
                        rv = ranges::any_of( arg.data, contains_heading );
                    }
                    else if constexpr( std::is_same_v< T, none_of > )
                    {
                        rv = ranges::none_of( arg.data, contains_heading );
                    }
                    else if constexpr( std::is_same_v< T, exactly > )
                    {
                        rv = ( rhs == arg.data );
                    }
                    else if constexpr( std::is_same_v< T, Intermediary > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                    }
                    else if constexpr( std::is_same_v< T, Uuid > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                        // TODO: Missing 'kmap' here... so impl. can't be done until I account for that.
                    }
                    else if constexpr( std::is_same_v< T, UuidSet > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                    }
                    else if constexpr( std::is_same_v< T, PredFn > )
                    {
                        KMAP_THROW_EXCEPTION_MSG( "TODO" );
                    }
                    else
                    {
                        static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                    }
                }
              , lhs );

    return rv;
}

// TODO: Shouldn't this return Result< UuidSet >, or is an empty set accounted for as a failed match by the caller?
// TODO: Redundancy with `operator==(SelectionVariant, Heading)?` Cannot this replace operator==?
auto match( Kmap const& kmap
          , UuidSet const& lhs
          , SelectionVariant const& rhs )
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    std::visit( [ & ]( auto const& arg ) mutable
                {
                    using T = std::decay_t< decltype( arg ) >;

                    auto const lhs_headings = [ & ] 
                    { 
                        return lhs
                             | ranges::views::transform( [ & ]( auto const& e ){ return KMAP_TRYE( nw->fetch_heading( e ) ); } ) 
                             | ranges::to< std::set >();
                    }();
                    auto const contains_heading = [ & ]( auto const& e ){ return lhs_headings.contains( e ); };

                    if constexpr( std::is_same_v< T, char const* > || std::is_same_v< T, std::string > )
                    {
                        rv = lhs
                           | ranges::views::filter( [ & ]( auto const& e ){ return KMAP_TRYE( nw->fetch_heading( e ) ) == arg; } )
                           | ranges::to< UuidSet >();
                    }
                    else if constexpr( std::is_same_v< T, all_of > )
                    {
                        if( ranges::all_of( arg.data, contains_heading ) )
                        {
                            rv = lhs
                               | ranges::views::filter( [ & ]( auto const& e ){ return arg.data.contains( KMAP_TRYE( nw->fetch_heading( e ) ) ); } )
                               | ranges::to< std::set >();
                        }
                    }
                    else if constexpr( std::is_same_v< T, any_of > )
                    {
                        rv = lhs
                           | ranges::views::filter( [ & ]( auto const& e ){ return arg.data.contains( KMAP_TRYE( nw->fetch_heading( e ) ) ); } )
                           | ranges::to< std::set >();
                    }
                    else if constexpr( std::is_same_v< T, none_of > )
                    {
                        rv = lhs
                            | ranges::views::filter( [ & ]( auto const& e ){ return !arg.data.contains( KMAP_TRYE( nw->fetch_heading( e ) ) ); } )
                            | ranges::to< std::set >();
                    }
                    else if constexpr( std::is_same_v< T, exactly > )
                    {
                        if( lhs_headings == arg.data )
                        {
                            rv = lhs;
                        }
                    }
                    else if constexpr( std::is_same_v< T, Intermediary > )
                    {
                        auto const ns = arg | view::to_node_set( kmap );

                        // set_insection
                        rv = lhs
                           | ranges::views::filter( [ & ]( auto const& e ){ return ns.contains( e ); } )
                           | ranges::to< std::set >();
                    }
                    else if constexpr( std::is_same_v< T, Uuid > )
                    {  
                        if( lhs.contains( arg ) )
                        {
                            rv = UuidSet{ arg };
                        }
                    }
                    else if constexpr( std::is_same_v< T, UuidSet > )
                    {
                        // set_insection
                        rv = lhs
                           | ranges::views::filter( [ & ]( auto const& e ){ return arg.contains( e ); } )
                           | ranges::to< std::set >();
                    }
                    else if constexpr( std::is_same_v< T, PredFn > )
                    {  
                        rv = lhs
                           | ranges::views::filter( [ & ]( auto const& e ){ return arg( e ); } )
                           | ranges::to< std::set >();
                    }
                    else
                    {
                        static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                    }
                }
              , rhs );

    return rv;
}

auto to_string( SelectionVariant const& sel )
    -> std::string
{
    return std::visit( []( auto const& arg ) -> std::string
                       {
                           using T = std::decay_t< decltype( arg ) >;

                           if constexpr( std::is_same_v< T, char const* >
                                      || std::is_same_v< T, std::string > )
                           {
                               return fmt::format( "'{}'", arg );
                           }
                           else if constexpr( std::is_same_v< T, all_of > )
                           {
                               return "all_of( TODO )";
                           }
                           else if constexpr( std::is_same_v< T, any_of > )
                           {
                               return "any_of( TODO )";
                           }
                           else if constexpr( std::is_same_v< T, none_of > )
                           {
                               return "none_of( TODO )";
                           }
                           else if constexpr( std::is_same_v< T, exactly > )
                           {
                               return "exactly( TODO )";
                           }
                           else if constexpr( std::is_same_v< T, Intermediary > )
                           {
                               return arg | act::to_string;
                           }
                           else if constexpr( std::is_same_v< T, Uuid > )
                           {
                               return kmap::to_string( arg );
                           }
                           else if constexpr( std::is_same_v< T, UuidSet > )
                           {
                               auto const joined = arg 
                                                 | ranges::views::transform( []( auto const& id ){ return kmap::to_string( id ); } )
                                                 | ranges::views::join( ',' )
                                                 | ranges::to< std::string >();

                               return fmt::format( "({})\n", joined );
                           }
                           else if constexpr( std::is_same_v< T, PredFn > )
                           {
                               return "pred( TODO )";
                           }
                           else
                           {
                               static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                           }
                       }
                     , sel );
}


// Select all leaves: make_view( kmap, option_node ) | desc | leaf
// Select all Option type nodes: make_view( kmap, option_node ) | desc | child( all_of( "description", any_of( "action", "value" ) ) );
// auto Intermediary::begin()
//     -> UuidSet::iterator
// {
//     return nodes.begin();
// }

// auto Intermediary::end()
//     -> UuidSet::iterator
// {
//     return nodes.end();
// }

// auto Intermediary::begin() const
//     -> UuidSet::const_iterator
// {
//     return nodes.begin();
// }

// auto Intermediary::end() const
//     -> UuidSet::const_iterator
// {
//     return nodes.end();
// }

// auto Intermediary::cbegin() const
//     -> UuidSet::const_iterator
// {
//     return nodes.cbegin();
// }

// auto Intermediary::cend() const
//     -> UuidSet::const_iterator
// {
//     return nodes.cend();
// }

auto make( Uuid const& n ) // TODO: Rename to view::make_root()?
    -> Intermediary
{
    return Intermediary{ {}, { n } };
}

auto make( UuidSet const& ns )
    -> Intermediary
{
    return Intermediary{ {}, ns };
}

auto Alias::operator()( Kmap const& kmap
                      , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const children = nw->fetch_children( node );
    auto const alias_children = children
                              | ranges::views::filter( [ &nw ]( auto const& e ){ return nw->alias_store().is_alias( e ); } )
                              | ranges::to< std::set >();
    auto const process_aliases = [ & ]( auto const& parent
                                      , auto const& aliases )
    {
        if( predicate )
        {
            auto const dispatch = 
                util::Dispatch{ [ & ]( std::string const& pred )
                                {
                                    return aliases
                                         | ranges::views::filter( [ & ]( auto const& e ){ return pred == KMAP_TRYE( nw->fetch_heading( e ) ); } ) 
                                         | ranges::to< std::set >();
                                } 
                              , [ & ]( Uuid const& pred )
                                {
                                    if( aliases.contains( pred ) )
                                    {
                                        return UuidSet{ pred };
                                    }
                                    else
                                    {
                                        return UuidSet{};
                                    }
                                } 
                              , [ & ]( Src const& pred )
                                {
                                    auto const resolved = aliases
                                                        | ranges::views::transform( [ & ]( auto const& e ){ return nw->alias_store().resolve( e ); } )
                                                        | ranges::to< std::set >();
                                    if( resolved.contains( pred.value() ) )
                                    {
                                        return UuidSet{ pred.value() };
                                    }
                                    else
                                    {
                                        return UuidSet{};
                                    }
                                } 
                              , [ & ]( Dst const& pred )
                                {
                                    auto const resolved = aliases
                                                        | ranges::views::transform( [ & ]( auto const& e ){ return KTRYE( nw->fetch_parent( e ) ); } )
                                                        | ranges::views::transform( [ & ]( auto const& e ){ return nw->alias_store().resolve( e ); } )
                                                        | ranges::to< std::set >();
                                    if( resolved.contains( pred.value() ) )
                                    {
                                        return UuidSet{ pred.value() };
                                    }
                                    else
                                    {
                                        return UuidSet{};
                                    }
                                } 
                              , [ & ]( UuidSet const& pred )
                                {
                                    return aliases
                                         | ranges::views::filter( [ & ]( auto const& e ){ return pred.contains( e ); } )
                                         | ranges::to< std::set >();
                                } 
                              , [ & ]( Intermediary const& pred )
                                {
                                    auto const ns = pred | view::to_node_set( kmap );

                                    return aliases
                                         | ranges::views::filter( [ & ]( auto const& e ){ return ns.contains( e ); } )
                                         | ranges::to< std::set >();
                                } 
                              , [ & ]( PredFn const& pred )
                                {
                                    return aliases
                                         | ranges::views::filter( [ & ]( auto const& e ){ return pred( e ); } )
                                         | ranges::to< std::set >();
                                } };

            return std::visit( dispatch, predicate.value() );
        }
        else
        {
            return aliases;
        }
    };

    rv = process_aliases( node, alias_children );

    return rv;
}

auto Alias::create( Kmap& kmap
                  , Uuid const& parent ) const
    -> Result< UuidSet >
{
    using NoMatchVariant = std::variant< char const*
                                       , std::string
                                       , Dst
                                       , all_of
                                       , any_of
                                       , none_of
                                       , exactly
                                       , PredFn >;

    auto rv = KMAP_MAKE_RESULT( UuidSet );

    if( predicate )
    {
        auto const nw = KTRY( kmap.fetch_component< com::Network>() );
        auto const dispatch = 
            util::Dispatch{ [ & ]( NoMatchVariant const& pred ) -> Result< UuidSet >
                            {
                                return KMAP_MAKE_ERROR( error_code::network::invalid_path );
                            }
                          , [ & ]( Uuid const& pred ) -> Result< UuidSet >
                            {
                                return Result< UuidSet >{ UuidSet{ KTRY( nw->alias_store().create_alias( pred, parent ) ) } };
                            } 
                          , [ & ]( Src const& pred ) -> Result< UuidSet >
                            {
                                return Result< UuidSet >{ UuidSet{ KTRY( nw->alias_store().create_alias( pred.value(), parent ) ) } };
                            } 
                          , [ & ]( UuidSet const& pred ) -> Result< UuidSet >
                            {
                                auto rs = UuidSet{};
                                
                                for( auto const& e : pred )
                                {
                                    rs.emplace( KTRY( nw->alias_store().create_alias( e, parent ) ) );
                                }

                                return Result< UuidSet >{ rs };
                            } 
                          , [ & ]( Intermediary const& pred ) -> Result< UuidSet >
                            {
                                auto const ns = pred | view::to_node_set( kmap );
                                auto rs = UuidSet{};
                                
                                for( auto const& e : ns )
                                {
                                    rs.emplace( KTRY( nw->alias_store().create_alias( e, parent ) ) );
                                }

                                return Result< UuidSet >{ rs };
                            } };

        rv = KTRY( std::visit( dispatch, predicate.value() ) );
    }

    return rv;
}

auto Alias::to_string() const
    -> std::string
{
    if( predicate )
    {
        return fmt::format( "alias( TODO )"/*, view::to_string( predicate.value() )*/ );
    }
    else
    {
        return "alias";
    }
}

auto Ancestor::operator()( Kmap const& kmap
                         , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const check_pred = [ & ]( auto const& n )
    {
        if( selection )
        {
            return !match( kmap, { n }, selection.value() ).empty();
        }
        else
        {
            return true;
        }
    };
    auto p = nw->fetch_parent( node );

    while( p )
    {
        auto const& pv = p.value();

        if( check_pred( pv ) )
        {
            rv.emplace( pv );
        }

        p = nw->fetch_parent( pv );
    }

    return rv;
}

auto Ancestor::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "ancestor( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "ancestor";
    }
}

auto Attr::operator()( Kmap const& kmap
                     , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( auto const attrn = nw->fetch_attr_node( node )
      ; attrn )
    {
        rv = UuidSet{ attrn.value() };
    }

    return rv;
}

auto Attr::create( Kmap& kmap
                 , Uuid const& parent ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const db = KTRY( kmap.fetch_component< com::Database >() );

    rv = KTRY( com::create_attr_node( *db, parent ) );

    return rv;
}

auto Attr::to_string() const
    -> std::string
{
    return "attr";
}

auto Child::operator()( Kmap const& kmap
                      , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const process_children = [ & ]( auto const& parent
                                       , auto const& children )
    {
        if( selection )
        {
            auto const& osv = selection.value();

            return match( kmap, children, osv );
        }
        else
        {
            return children;
        }
    };

    rv = process_children( node, nw->fetch_children( node ) );

    return rv;
}

auto Child::create( Kmap& kmap
                  , Uuid const& parent ) const
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );

    if( selection )
    {
        auto const nw = KTRY( kmap.fetch_component< com::Network >() );

        rv = std::visit( [ & ]( auto const& arg ) -> Result< UuidSet >
                         {
                             using T = std::decay_t< decltype( arg ) >;

                             auto rset = KMAP_MAKE_RESULT( UuidSet );

                             if constexpr( std::is_same_v< T, char const* >
                                        || std::is_same_v< T, std::string > )
                             {
                                 rset = UuidSet{ KTRY( nw->create_child( parent, arg ) ) };
                             }
                             else if constexpr( std::is_same_v< T, all_of > )
                             {
                                 auto t = UuidSet{};

                                 for( auto const& e : arg.data )
                                 {
                                     t.emplace( KTRY( nw->create_child( parent, e ) ) );
                                 }

                                 rset = t;
                             }
                             else if constexpr( std::is_same_v< T, any_of > )
                             {
                                 KMAP_THROW_EXCEPTION_MSG( "invalid node qualifier" );
                                 // create "any_of" isn't a good request. It doesn't make sense. Always return error.
                                 // rv = KMAP_MAKE_ERROR( error_code::network::invalid_qualifier ); ?
                             }
                             else if constexpr( std::is_same_v< T, none_of > )
                             {
                                 KMAP_THROW_EXCEPTION_MSG( "invalid node qualifier" );
                                 // rv = KMAP_MAKE_ERROR( error_code::network::invalid_qualifier ); ?
                             }
                             else if constexpr( std::is_same_v< T, exactly > )
                             {
                                 // This one is tricky.
                                 // Take every element node that matches only fetch_descendant( dn, dr )?.
                                 // It's possible that this doesn't belong in the for loop, but just from the root.
                                 // Need to clarify exactly what "exactly" means.
                                 KMAP_THROW_EXCEPTION_MSG( "TODO" );
                             }
                             else if constexpr( std::is_same_v< T, Intermediary > )
                             {
                                 KMAP_THROW_EXCEPTION_MSG( "TODO" );
                             }
                             else if constexpr( std::is_same_v< T, Uuid > )
                             {
                                 KMAP_THROW_EXCEPTION_MSG( "TODO" );
                             }
                             else if constexpr( std::is_same_v< T, UuidSet > )
                             {
                                 KMAP_THROW_EXCEPTION_MSG( "TODO" );
                             }
                             else if constexpr( std::is_same_v< T, PredFn > )
                             {  
                                 KMAP_THROW_EXCEPTION_MSG( "TODO" );
                             }
                             else
                             {
                                 static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                             }

                             return rset;
                         }
                         , selection.value() );
    }

    return rv;
}

auto Child::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "child( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "child";
    }
}

auto Desc::operator()( Kmap const& kmap
                     , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( e != node );
                // TODO: assert each node in rv is actually a desc
            }
        })
    ;

    if( selection )
    {
        auto const vr = std::visit( [ & ]( auto const& arg ) -> UuidSet
                                    {
                                        using T = std::decay_t< decltype( arg ) >;

                                        auto rset = UuidSet{};

                                        if constexpr( std::is_same_v< T, char const* >
                                                   || std::is_same_v< T, std::string > )
                                        {
                                            if( auto const dr = decide_path( kmap, node, node, arg )
                                              ; dr )
                                            {
                                                auto const& d = dr.value();

                                                rset.insert( d.begin(), d.end() );
                                            }

                                        }
                                        else if constexpr( std::is_same_v< T, all_of > )
                                        {
                                            auto const all_of_sets = arg.data
                                                                   | ranges::views::transform( [ & ]( auto const& e )
                                                                     {
                                                                        auto as = UuidSet{};
                                                                        if( auto const dr = decide_path( kmap, node, node, e )
                                                                          ; dr )
                                                                        {
                                                                            auto const& d = dr.value();
                                                                            as.insert( d.begin(), d.end() );
                                                                        }
                                                                        return as;
                                                                     } )
                                                                   | ranges::to< std::set< UuidSet > >();

                                            if( arg.data.size() == ranges::count_if( all_of_sets, []( auto const& e ){ return !e.empty(); } ) )
                                            {
                                                for( auto const& e : all_of_sets )
                                                {
                                                    rset.insert( e.begin(), e.end() );
                                                }
                                            }
                                        }
                                        else if constexpr( std::is_same_v< T, any_of > )
                                        {
                                            auto rset = UuidSet{};

                                            for( auto const& e : arg.data )
                                            {
                                                if( auto const dr = decide_path( kmap, node, node, e )
                                                  ; dr )
                                                {
                                                    auto const& d = dr.value();
                                                    
                                                    rset.insert( d.begin(), d.end() );
                                                }
                                            }

                                            return rset;
                                        }
                                        else if constexpr( std::is_same_v< T, none_of > )
                                        {
                                            if( auto const allsetr = fetch_descendants( kmap, node )
                                              ; allsetr )
                                            {
                                                auto const& allset = allsetr.value();
                                                auto noneset = UuidSet{};
                                                
                                                for( auto const& e : arg.data )
                                                {
                                                    if( auto const dr = decide_path( kmap, node, node, e )
                                                      ; dr )
                                                    {
                                                        auto const& d = dr.value();

                                                        noneset.insert( d.begin(), d.end() );
                                                    }
                                                }

                                                rset = ranges::views::set_difference( allset, noneset )
                                                     | ranges::to< UuidSet >();
                                            } 
                                        }
                                        else if constexpr( std::is_same_v< T, exactly > )
                                        {
                                            // This one is tricky.
                                            // Take every element node that matches only fetch_descendant( dn, dr )?.
                                            // It's possible that this doesn't belong in the for loop, but just from the root.
                                            // Need to clarify exactly what "exactly" means in terms of view::desc.
                                            // I suppose it means that exactly( qualifiers ) descendants exists and no other descendants exist.
                                            KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                        }
                                        else if constexpr( std::is_same_v< T, Intermediary > )
                                        {
                                            auto const ns = arg | view::to_node_set( kmap );
                                            if( auto const dr = fetch_descendants( kmap, node, [ &ns ]( auto const& e ){ return ns.contains( e ); } )
                                              ; dr )
                                            {
                                                auto const& d = dr.value();

                                                rset.insert( d.begin(), d.end() );
                                            }
                                        }
                                        else if constexpr( std::is_same_v< T, Uuid > )
                                        {
                                            if( auto const dr = fetch_descendants( kmap, node, [ &arg ]( auto const& e ){ return e == arg; } )
                                              ; dr )
                                            {
                                                auto const& d = dr.value();

                                                rset.insert( d.begin(), d.end() );
                                            }
                                        }
                                        else if constexpr( std::is_same_v< T, UuidSet > )
                                        {
                                            if( auto const dr = fetch_descendants( kmap, node, [ &arg ]( auto const& e ){ return arg.contains( e ); } )
                                              ; dr )
                                            {
                                                auto const& d = dr.value();

                                                rset.insert( d.begin(), d.end() );
                                            }
                                        }
                                        else if constexpr( std::is_same_v< T, PredFn > )
                                        {  
                                            if( auto const dr = fetch_descendants( kmap, node, [ &arg ]( auto const& e ){ return arg( e ); } )
                                              ; dr )
                                            {
                                                auto const& d = dr.value();

                                                rset.insert( d.begin(), d.end() );
                                            }
                                        }
                                        else
                                        {
                                            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                        }

                                        return rset;
                                    }
                                    , selection.value() );

        rv.insert( vr.begin(), vr.end() );
    }
    else
    {
        rv = KMAP_TRYE( fetch_descendants( kmap, node ) );
    }

    rv = rv
       | ranges::views::filter( [ &node ]( auto const& e ){ return e != node; } )
       | ranges::to< UuidSet >();

    return rv;
}

// auto Desc::operator()( OperatorVariant const& op ) const
//     -> Intermediary
// {
//     auto rv = Intermediary{};

//     return rv;
// }

auto Desc::create( Kmap& kmap
                 , Uuid const& root ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    if( selection )
    {
        auto const& sv = selection.value();

        if( std::holds_alternative< char const* >( sv ) )
        {
            rv = KTRY( create_descendants( kmap, root, root, std::get< char const* >( sv ) ) ).back();
        }
        else if( std::holds_alternative< std::string >( sv ) )
        {
            rv = KTRY( create_descendants( kmap, root, root, std::get< std::string >( sv ) ) ).back();
        }
        // TODO: Others?
    }

    return rv;
}

auto Desc::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "desc( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "desc";
    }
}

auto DirectDesc::operator()( Kmap const& kmap
                           , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( e != node );
            }
        })
    ;

    if( selection )
    {
        auto const vr = std::visit( [ & ]( auto const& arg ) -> UuidSet
                                    {
                                        using T = std::decay_t< decltype( arg ) >;

                                        auto const make_direct = []( std::string const& s ){ return fmt::format( ".{}", s ); };
                                        auto rset = UuidSet{};

                                        if constexpr( std::is_same_v< T, char const* >
                                                   || std::is_same_v< T, std::string > )
                                        {
                                            if( auto const dr = decide_path( kmap, node, node, make_direct( arg ) )
                                              ; dr )
                                            {
                                                auto const& d = dr.value();

                                                rset.insert( d.begin(), d.end() );
                                            }

                                        }
                                        else if constexpr( std::is_same_v< T, all_of > )
                                        {
                                            auto const all_of_sets = arg.data
                                                                   | ranges::views::transform( [ & ]( auto const& e )
                                                                     {
                                                                        auto as = UuidSet{};
                                                                        if( auto const dr = decide_path( kmap, node, node, make_direct( e ) )
                                                                          ; dr )
                                                                        {
                                                                            auto const& d = dr.value();
                                                                            as.insert( d.begin(), d.end() );
                                                                        }
                                                                        return as;
                                                                     } )
                                                                   | ranges::to< std::set< UuidSet > >();

                                            if( arg.data.size() == ranges::count_if( all_of_sets, []( auto const& e ){ return !e.empty(); } ) )
                                            {
                                                for( auto const& e : all_of_sets )
                                                {
                                                    rset.insert( e.begin(), e.end() );
                                                }
                                            }
                                            for( auto const& e : arg.data
                                                               | ranges::views::transform( make_direct ) )
                                            {
                                                if( auto const dr = decide_path( kmap, node, node, e )
                                                  ; dr )
                                                {
                                                    auto const& d = dr.value();

                                                    rset.insert( d.begin(), d.end() );
                                                }
                                            }
                                        }
                                        else if constexpr( std::is_same_v< T, any_of > )
                                        {
                                            auto rset = UuidSet{};

                                            for( auto const& e : arg.data
                                                               | ranges::views::transform( make_direct ) )
                                            {
                                                if( auto const dr = decide_path( kmap, node, node, e )
                                                  ; dr )
                                                {
                                                    auto const& d = dr.value();

                                                    rset.insert( d.begin(), d.end() );
                                                }
                                            }

                                            return rset;
                                        }
                                        else if constexpr( std::is_same_v< T, none_of > )
                                        {
                                            // This one is tricky.
                                            // Take every element node that doesn't match fetch_descendant( dn, dr )? Tricky.
                                            // It's possible that this doesn't belong in the for loop, but just from the root.
                                            // I suppose this means gathering all matches, then traversing all descendants and taking the set difference.

                                            KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                        }
                                        else if constexpr( std::is_same_v< T, exactly > )
                                        {
                                            // This one is tricky.
                                            // Take every element node that matches only fetch_descendant( dn, dr )?.
                                            // It's possible that this doesn't belong in the for loop, but just from the root.
                                            // Need to clarify exactly what "exactly" means.
                                            KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                        }
                                        else if constexpr( std::is_same_v< T, Intermediary > )
                                        {
                                            KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                        }
                                        else if constexpr( std::is_same_v< T, Uuid > )
                                        {
                                            if( nw->is_child( node, arg ) )
                                            {
                                                rset.emplace( arg );
                                            }
                                        }
                                        else if constexpr( std::is_same_v< T, UuidSet > )
                                        {
                                            KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                        }
                                        else if constexpr( std::is_same_v< T, PredFn > )
                                        {  
                                            KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                        }
                                        else
                                        {
                                            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                        }

                                        return rset;
                                    }
                                    , selection.value() );

        rv.insert( vr.begin(), vr.end() );
    }
    else
    {
        rv = KMAP_TRYE( fetch_descendants( kmap, node ) ); // All descendants are direct when unqualified.
    }

    rv = rv
       | ranges::views::filter( [ &node ]( auto const& e ){ return e != node; } )
       | ranges::to< UuidSet >();

    return rv;
}

auto DirectDesc::create( Kmap& kmap
                       , Uuid const& root ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    if( selection )
    {
        auto const& sv = selection.value();

        if( std::holds_alternative< char const* >( sv ) )
        {
            rv = KTRY( create_descendants( kmap, root, root, fmt::format( ".{}", std::get< char const* >( sv ) ) ) ).back();
        }
        else if( std::holds_alternative< std::string >( sv ) )
        {
            rv = KTRY( create_descendants( kmap, root, root, fmt::format( ".{}", std::get< std::string >( sv ) ) ) ).back();
        }
        // TODO: Others?
    }

    return rv;
}

auto DirectDesc::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "direct_desc( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "direct_desc";
    }
}

auto Leaf::operator()( Kmap const& kmap
                     , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( nw->fetch_children( node ).empty() )
    {
        rv = UuidSet{ node };
    }

    return rv;
}

auto Leaf::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "leaf( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "leaf";
    }
}

auto Lineage::operator()( Kmap const& km
                        , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto ancestry = [ & ]
    {
        if( selection )
        {
            return view::make( node )
                 | view::ancestor( selection.value() )
                 | view::to_node_set( km );
        }
        else
        {
            return view::make( node )
                 | view::ancestor
                 | view::to_node_set( km );
        }
    }();

    ancestry.emplace( node );

    rv = ancestry;

    return rv;
}

auto Lineage::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "lineage( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "lineage";
    }
}

auto RLineage::operator()( Kmap const& kmap
                         , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const is_ancestor = view::make( leaf )
                           | view::ancestor( node )
                           | view::exists( kmap );

    if( node == leaf || is_ancestor )
    {
        rv.emplace( leaf );
        rv.emplace( node );

        for( decltype( nw->fetch_parent( leaf ) ) anc = leaf
           ; anc && anc.value() != node
           ; anc = nw->fetch_parent( anc.value() ) )
        {
            rv.emplace( anc.value() );
        }
    }

    return rv;
}

SCENARIO( "view::rlineage basics", "[node_view][lineage]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "database", "root_node" );

    auto& kmap = Singleton::instance();
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );
    auto const root = kmap.root_node_id();

    GIVEN( "root" )
    {
        THEN( "root-to-root lineage exists" )
        {
            REQUIRE(( view::make( root ) | view::rlineage( root ) | view::exists( kmap ) ));
        }
        THEN( "root-to-invalid-node lineage doesn't exists" )
        {
            REQUIRE( !( view::make( root ) | view::rlineage( gen_uuid() ) | view::exists( kmap ) ) );
        }

        WHEN( "root has child ")
        {
            auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );

            THEN( "root-to-child lineage exists" )
            {
                REQUIRE(( view::make( root ) | view::rlineage( c1 ) | view::exists( kmap ) ));
            }
        }
    }
    GIVEN( "root, child, and grandchild" )
    {
        auto const c1 = REQUIRE_TRY( nw->create_child( root, "1" ) );
        auto const c11 = REQUIRE_TRY( nw->create_child( c1, "1" ) );

        THEN( "root-to-child lineage exists" )
        {
            auto const v = view::make( root ) | view::rlineage( c1 );
            REQUIRE(( v | view::exists( kmap ) ));
            REQUIRE(( ( v | view::to_node_set( kmap ) ) == UuidSet{ root, c1 } ));
        }
        THEN( "root-to-grandchild lineage exists" )
        {
            auto const v = view::make( root ) | view::rlineage( c11 );
            REQUIRE(( v | view::exists( kmap ) ));
            REQUIRE(( ( v | view::to_node_set( kmap ) ) == UuidSet{ root, c1, c11 } ));
        }
        THEN( "child-to-grandchild lineage exists" )
        {
            auto const v = view::make( c1 ) | view::rlineage( c11 );
            REQUIRE(( v | view::exists( kmap ) ));
            REQUIRE(( ( v | view::to_node_set( kmap ) ) == UuidSet{ c1, c11 } ));
        }
        THEN( "grandchild-to-child lineage does NOT exists" )
        {
            REQUIRE( !( view::make( c11 ) | view::rlineage( c1 ) | view::exists( kmap ) ) );
        }
        THEN( "grandchild-to-root lineage does NOT exists" )
        {
            REQUIRE( !( view::make( c11 ) | view::rlineage( root ) | view::exists( kmap ) ) );
        }
    }
    GIVEN( "multibranch tree" )
    {
        // TODO
    }
}

auto RLineage::to_string() const
    -> std::string
{
    return fmt::format( "rlineage( {} )", kmap::to_string( leaf ) );
}

auto Parent::operator()( Kmap const& kmap
                       , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( auto const p = nw->fetch_parent( node )
      ; p )
    {
        rv = UuidSet{ p.value() };
    }

    return rv;
}

auto Parent::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "parent( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "parent";
    }
}

auto Resolve::operator()( Kmap const& kmap
                        , Uuid const& node ) const
    -> UuidSet
{
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    return { nw->alias_store().resolve( node ) };
}

auto Resolve::to_string() const
    -> std::string
{
    return "resolve";
}

// TODO: Can I actually define sibling directly in terms of existing operators? That is, without having to create the Struct Sibling?
//       Just: auto const sibling = view::parent | view::child( view::none_of( <node> ) );
//       I suppose the <node> is the kicker, as to why I can't.
//       ...Unless I introduce a "placeholder_node" concept in which the real node value gets substituted appropriately...
auto Sibling::operator()( Kmap const& kmap
                        , Uuid const& node ) const
    -> UuidSet
{
    KMAP_THROW_EXCEPTION_MSG( "TODO" );
    // TODO: Requires view::none_of( uuid ) support.
    // return view::make( node )
    //      | view::parent
    //      | view::child( view::none_of( node ) )
    //      | view::to_node_set( kmap );
}

auto Sibling::to_string() const
    -> std::string
{
    return "sibling";
}

auto SiblingIncl::operator()( Kmap const& kmap
                            , Uuid const& node ) const
    -> UuidSet
{
    return view::make( node )
         | view::parent
         | view::child
         | view::to_node_set( kmap );
}

auto SiblingIncl::to_string() const
    -> std::string
{
    return "sibling_incl";
}

auto Single::to_string() const
    -> std::string
{
    return "single";
}

auto Tag::operator()( Kmap const& kmap
                    , Uuid const& node ) const
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const tags = view::make( node )
                    | view::attr
                    | view::child( "tag" )
                    | view::alias
                    | view::to_node_set( kmap );

    if( selection )
    {
        auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
        auto const selected = std::visit( [ & ]( auto const& arg ) -> Result< UuidSet >
                                          {
                                              using T = std::decay_t< decltype( arg ) >;

                                              auto sel = KMAP_MAKE_RESULT( UuidSet );
                                              auto const rtags = tags
                                                               | ranges::views::transform( [ &nw ]( auto const& t ){ return nw->alias_store().resolve( t ); } )
                                                               | ranges::to< std::set >();
                                              auto const tstore = KTRY( kmap.fetch_component< com::TagStore >() );

                                              if constexpr( std::is_same_v< T, char const* >
                                                         || std::is_same_v< T, std::string > )
                                              {
                                                  auto const ft = KTRY( tstore->fetch_tag( arg ) );

                                                  sel = tags
                                                      | ranges::views::filter( [ & ]( auto const& t ){ return nw->alias_store().resolve( t ) == ft; } )
                                                      | ranges::to< UuidSet >();
                                              }
                                              else if constexpr( std::is_same_v< T, all_of > )
                                              {
                                                  KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                              }
                                              else if constexpr( std::is_same_v< T, any_of > )
                                              {
                                                  KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                              }
                                              else if constexpr( std::is_same_v< T, none_of > )
                                              {
                                                  auto const tag_matches = arg.data
                                                                         | ranges::views::transform( [ & ]( auto const& t ){ return tstore->fetch_tag( t ); } )
                                                                         | ranges::views::filter( []( auto const& rt ){ return rt.has_value(); } )
                                                                         | ranges::views::transform( []( auto const& t ){ return t.value(); } )
                                                                         | ranges::to< UuidSet >();

                                                  sel = tags
                                                      | ranges::views::filter( [ & ]( auto const& t ){ return !tag_matches.contains( nw->alias_store().resolve( t ) ); } )
                                                      | ranges::to< UuidSet >();
                                              }
                                              else if constexpr( std::is_same_v< T, exactly > )
                                              {
                                                  KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                              }
                                              else if constexpr( std::is_same_v< T, Intermediary > )
                                              {
                                                  auto ts = UuidSet{};
                                                  for( auto const& tag : arg | view::to_node_set( kmap ) )
                                                  {
                                                      if( rtags.contains( tag ) )
                                                      {
                                                          ts.emplace( tag );
                                                      }
                                                  }
                                                  sel = ts;
                                              }
                                              else if constexpr( std::is_same_v< T, Uuid > )
                                              {
                                                  if( rtags.contains( arg ) )
                                                  {
                                                      sel = UuidSet{ arg };
                                                  }
                                              }
                                              else if constexpr( std::is_same_v< T, UuidSet > )
                                              {
                                                  auto ts = UuidSet{};
                                                  for( auto const& tag : arg )
                                                  {
                                                      if( rtags.contains( tag ) )
                                                      {
                                                          ts.emplace( tag );
                                                      }
                                                  }
                                                  sel = ts;
                                              }
                                              else if constexpr( std::is_same_v< T, PredFn > )
                                              {  
                                                  KMAP_THROW_EXCEPTION_MSG( "TODO" );
                                              }
                                              else
                                              {
                                                  static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                              }

                                              return sel;
                                          }
                                        , selection.value() );
        
        if( selected )
        {
            rv = selected.value();
        }
    }
    else
    {
        rv = tags;
    }

    return rv;
}

auto Tag::create( Kmap& kmap
                , Uuid const& parent ) const
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );

    if( selection )
    {
        rv = std::visit( [ & ]( auto const& arg ) -> Result< UuidSet >
                         {
                             using T = std::decay_t< decltype( arg ) >;

                             auto rset = KMAP_MAKE_RESULT( UuidSet );
                             auto const tstore = KTRY( kmap.fetch_component< com::TagStore >() );

                             if constexpr( std::is_same_v< T, char const* >
                                        || std::is_same_v< T, std::string > )
                             {
                                 rset = UuidSet{ KTRY( tstore->tag_node( parent, arg ) ) };
                             }
                             else if constexpr( std::is_same_v< T, all_of > )
                             {
                                 auto t = UuidSet{};

                                 for( auto const& e : arg.data )
                                 {
                                     t.emplace( KTRY( tstore->tag_node( parent, e ) ) );
                                 }

                                 rset = t;
                             }
                             else if constexpr( std::is_same_v< T, any_of > )
                             {
                                 KMAP_THROW_EXCEPTION_MSG( "invalid node qualifier" );
                             }
                             else if constexpr( std::is_same_v< T, none_of > )
                             {
                                 KMAP_THROW_EXCEPTION_MSG( "invalid node qualifier" );
                             }
                             else if constexpr( std::is_same_v< T, exactly > )
                             {
                                 // This one is tricky.
                                 // Take every element node that matches only fetch_descendant( dn, dr )?.
                                 // It's possible that this doesn't belong in the for loop, but just from the root.
                                 // Need to clarify exactly what "exactly" means.
                                 KMAP_THROW_EXCEPTION_MSG( "TODO" );
                             }
                             else if constexpr( std::is_same_v< T, Intermediary > )
                             {
                                 auto ts = UuidSet{};
                                 for( auto const& tag : arg | to_node_set( kmap ) )
                                 {
                                    ts.emplace( KTRY( tstore->tag_node( parent, tag ) ) );
                                 }
                                 rset = ts;
                             }
                             else if constexpr( std::is_same_v< T, Uuid > )
                             {
                                 rset = UuidSet{ KTRY( tstore->tag_node( parent, arg ) ) };
                             }
                             else if constexpr( std::is_same_v< T, UuidSet > )
                             {
                                 auto ts = UuidSet{};
                                 for( auto const& tag : arg )
                                 {
                                    ts.emplace( KTRY( tstore->tag_node( parent, tag ) ) );
                                 }
                                 rset = ts;
                             }
                             else if constexpr( std::is_same_v< T, PredFn > )
                             {  
                                 KMAP_THROW_EXCEPTION_MSG( "TODO" );
                             }
                             else
                             {
                                 static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                             }

                             return rset;
                         }
                         , selection.value() );
    }

    return rv;
}

auto Tag::to_string() const
    -> std::string
{
    if( selection )
    {
        return fmt::format( "tag( {} )", view::to_string( selection.value() ) );
    }
    else
    {
        return "tag";
    }
}

auto operator|( Intermediary const& i
              , Alias const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Ancestor const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Attr const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Child const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Count const& op )
    -> uint32_t
{
    return ( i | to_node_set( op.kmap ) ).size();
}

auto operator|( Intermediary const& i
              , Desc const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , DirectDesc const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , EraseNode const& op )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( op.kmap.fetch_component< com::Network >() );
    auto const nodes = i | to_node_set( op.kmap );

    for( auto const& node : nodes )
    {
        KTRY( nw->erase_node( node ) );
    }

    rv = outcome::success();

    return rv;
}

auto operator|( Intermediary const& i
              , Exists const& op )
    -> bool
{
    auto const ns = i | to_node_set( op.kmap );
    auto const nw = KTRYE( op.kmap.fetch_component< com::Network >() );

    return !ns.empty()
        && ranges::all_of( ns, [ &nw ]( auto const& n ){ return nw->exists( n ); } );
}

auto operator|( Intermediary const& i
              , Leaf const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Lineage const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , RLineage const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Parent const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Resolve const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Sibling const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , SiblingIncl const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , ToHeadingSet const& ths_op )
    -> std::set< std::string >
{
    auto const ns = i | to_node_set( ths_op.kmap );
    auto const nw = KTRYE( ths_op.kmap.fetch_component< com::Network >() );

    return ns
         | ranges::views::transform( [ &nw ]( auto const& n ){ return KTRYE( nw->fetch_heading( n ) ); } )
         | ranges::to< std::set >;
}

auto operator|( Intermediary const& i
              , ToNodeSet const& tns_op )
    -> UuidSet
{
    auto rv = i.root;

    for( auto const& op : i.op_chain )
    {
        if( rv.empty() )
        {
            break;
        }

        rv = std::visit( [ & ]( auto const& arg )
                         { 
                             using T = std::decay_t< decltype( arg ) >;

                             auto ns = decltype( rv ){};

                             if constexpr( std::is_same_v< T, Single > )
                             {
                                 if( rv.size() == 1 )
                                 {
                                     ns = rv;
                                 }
                             }
                             else
                             {
                                 for( auto const& n : rv )
                                 {
                                     auto res = arg( tns_op.kmap, n );

                                     ns.insert( res.begin(), res.end() );
                                 }
                                //  static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                             }

                             return ns;
                         }
                         , op );
    }

    return rv;
}

auto operator|( Intermediary const& i
              , FetchNode const& op )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const ns = i | to_node_set( op.kmap );

    if( ns.size() == 1 )
    {
        rv = *ns.begin();
    }
    else if( ns.size() > 1 )
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::network::ambiguous_path, i | act::to_string );
    }
    else // size == 0
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::network::invalid_node, i | act::to_string );
    }

    return rv;
}

#if 0
// The only difference, I see, between CreateNode and FetchOrCreateNode is in the leaf. If the leaf was created, it returns true. Otherwise, it returns false.
auto operator|( Intermediary const& i
              , CreateNode const& c_op )
    -> Result< Uuid >
{
    KMAP_ENSURE( !i.op_chain.empty(), error_code::network::invalid_path );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto ns = i.root;

    for( auto const& op : i.op_chain
                        | ranges::views::drop_last( 1 ) )
    {
        if( ns.empty() )
        {
            break;
        }

        auto const tns = ns;
        auto cumulative = decltype( ns ){};

        for( auto const& node : tns )
        {
            auto const fd = std::visit( [ & ]( auto const& vop )
                                        { 
                                            using T = std::decay_t< decltype( vop ) >;

                                            if constexpr( std::is_same_v< T, Single > )
                                            {
                                                return UuidSet{ node };
                                            }
                                            else
                                            {
                                                return vop( c_op.kmap, node );
                                            }
                                        }
                                      , op );
            if( fd.empty() )
            {
                auto const r = KMAP_TRY( create_lineage( c_op.kmap, node, op ) );

                cumulative.emplace( r );
            }
            else
            {
                cumulative.insert( fd.begin(), fd.end() );
            }
        }

        ns = cumulative;
    }

    // TODO: So, one problem here is in the "create" phase. If we create and end up with multiple created paths, we return an error, but the paths
    //       were still created. Outside of undo, I don't see a good way around this, given the requirements of this function, and that the function
    //       doesn't "know" about success or failure until after creation. I think undo is the only way to make this sound.
    if( ns.size() == 1 )
    {
        rv = KMAP_TRY( create_lineage( c_op.kmap, *ns.begin(), i.op_chain.back() ) );
    }
    else if( ns.size() > 1 )
    {
        rv = KMAP_MAKE_ERROR( error_code::network::ambiguous_path );
        // TODO: undo.... requires nested undo, no?
    }
    else // size == 0
    {
        rv = KMAP_MAKE_ERROR( error_code::network::invalid_node );
        // TODO: undo.... requires nested undo, no?
    }

    return rv;
}
#endif // 0

auto operator|( Intermediary const& i
              , CreateNode const& c_op )
    -> Result< UuidSet >
{
    KMAP_ENSURE( !i.op_chain.empty(), error_code::network::invalid_path );

    auto rv = KMAP_MAKE_RESULT( UuidSet );
    auto ns = i.root;

    for( auto const& op : i.op_chain
                        | ranges::views::drop_last( 1 ) )
    {
        if( ns.empty() )
        {
            break;
        }

        auto const tns = ns;
        auto cumulative = decltype( ns ){};

        for( auto const& node : tns )
        {
            // Fetch node.
            auto const fd = std::visit( [ & ]( auto const& vop )
                                        { 
                                            using T = std::decay_t< decltype( vop ) >;

                                            if constexpr( std::is_same_v< T, Single > ) // TODO: Should probably move this logic into Single::operator()()
                                            {
                                                if( ns.size() == 1 )
                                                {
                                                    return UuidSet{ node };
                                                }
                                                else
                                                {
                                                    return UuidSet{};
                                                }
                                            }
                                            else
                                            {
                                                return vop( c_op.kmap, node );
                                            }
                                        }
                                      , op );
            // Create if not found.
            if( fd.empty() )
            {
                auto const r = KMAP_TRY( create_lineages( c_op.kmap, node, op ) );

                cumulative.insert( r.begin(), r.end() );
            }
            else
            {
                cumulative.insert( fd.begin(), fd.end() );
            }
        }

        ns = cumulative;
    }

    // TODO: So, one problem here is in the "create" phase. If we create and end up with multiple created paths, we return an error, but the paths
    //       were still created. Outside of undo, I don't see a good way around this, given the requirements of this function, and that the function
    //       doesn't "know" about success or failure until after creation. I think undo is the only way to make this sound.
    if( ns.size() == 1 )
    {
        rv = KMAP_TRY( create_lineages( c_op.kmap, *ns.begin(), i.op_chain.back() ) );
    }

    if( ns.size() == 0 )
    {
        rv = KMAP_MAKE_ERROR( error_code::network::invalid_node );
        // TODO: undo.... requires nested undo, no?
    }

    return rv;
}

#if 0
auto create_lineage( Kmap& kmap
                   , Uuid const& root
                   , OperatorVariant const& op )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const lineages = KMAP_TRY( create_lineages( kmap, root, op ) );

    if( lineages.size() == 1 )
    {
        rv = *lineages.begin();
    }
    else
    {
        rv = KMAP_MAKE_ERROR( error_code::network::ambiguous_path );
    }

    return rv;
}
#endif // 0

// TODO: Rename to just "create"?
auto create_lineages( Kmap& kmap
                    , Uuid const& root
                    , OperatorVariant const& op )
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );

    rv = std::visit( [ & ]( auto const& vop ) -> Result< UuidSet >
                     { 
                         auto vrv = KMAP_MAKE_RESULT_EC( UuidSet, error_code::network::invalid_path );

                        // TODO: Better said: if constexpr( requires{ vop.create( kmap, root ); } ), else return error?
                         using T = std::decay_t< decltype( vop ) >;
                         // if returns arg.empty(), and arg is child, create child.
                         // elif returns arg.empty(), and arg is desc, create_descendents
                         if constexpr( std::is_same_v< T, Child > 
                                    || std::is_same_v< T, Desc >
                                    || std::is_same_v< T, DirectDesc >
                                    || std::is_same_v< T, Attr >
                                    || std::is_same_v< T, Tag > )
                         {
                             vrv = UuidSet{ KTRY( vop.create( kmap, root ) ) };
                         }
                         else if constexpr( std::is_same_v< T, Alias > )
                         {
                             vrv = KTRY( vop.create( kmap, root ) ); // TODO: Why is this in a separate category from the others? Even if it's correct, at least add a comment explaining why.
                         }
                         else if constexpr( std::is_same_v< T, Single > )
                         {
                             vrv = UuidSet{ root }; // Effectively a NOP for "create".
                         }
                         else if constexpr( std::is_same_v< T, Ancestor >
                                         || std::is_same_v< T, Leaf > // TODO: Shouldn't this one create? I think it's incorrect to be in the do-nothing
                                         || std::is_same_v< T, Sibling > // TODO: Shouldn't this one create? I think it's incorrect to be in the do-nothing
                                         || std::is_same_v< T, SiblingIncl > // TODO: Shouldn't this one create? I think it's incorrect to be in the do-nothing
                                         || std::is_same_v< T, Lineage >
                                         || std::is_same_v< T, RLineage >
                                         || std::is_same_v< T, Resolve >
                                         || std::is_same_v< T, Parent > )
                         {
                            // Nothing to do.
                         }
                         else
                         {
                            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                         }

                         return vrv;
                     }
                     , op );

    return rv;
}

/**
 * TODO: We use a visitor to create if not exists. Much of this will be redundant with view::create_node, save for the last node.
 *       Need to find a way to unify these. Basically, view::create_node is fetch_or_create + don't create final node if it already exists.
 *       Perhaps a simple flag as a member, FetchOrCreateNode::create_last_if_nonexistent = true, can be used. Then view::create_node can call with = false.
 *       Something like that to abstract the behavior.
 * TODO: Another issue: attribute children are special. To avoid recursion, they do not have attributes themselves. This means that statements like:
 *       `view::make_view( kmap ) | view::attr | view::child( "order" ) | view::fetch_or_create_node( kmap );` - won't behave correctly because
 *       `view::child( "order" )` isn't aware that it's an attribute's descendant. It will attempt to create a child like normal.
 *       There are a couple of ways around this. One is to have kmap::create_child() aware that `parent` is in an attribute lineage, and create the proper node
 *       without attributes. This would be the opaque/implicit solution. The explitic solution would be to have the following check if the current node is in an attr lineage
 *       and call a separate function.
 *       It seems to me less of a headache, burden, and source of future error, to consolate the functionality into the implicit solution.
 *       That is, kmap::create_node just "does the right thing".
 */
auto operator|( Intermediary const& i
              , FetchOrCreateNode const& foc_op )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto ns = i.root;

    for( auto const& op : i.op_chain )
    {
        if( ns.empty() )
        {
            break;
        }

        auto const tns = ns;
        auto cumulative = decltype( ns ){};

        for( auto const& node : tns )
        {
            auto const r = std::visit( [ & ]( auto const& vop ) -> Result< UuidSet >
                                       { 
                                           using T = std::decay_t< decltype( vop ) >;

                                           auto vrv = KMAP_MAKE_RESULT( UuidSet );
                                           auto const ar = [ & ]
                                           {
                                               if constexpr( std::is_same_v< T, Single > )
                                               {
                                                   return UuidSet{ node };
                                               }
                                               else
                                               {
                                                   return vop( foc_op.kmap, node );
                                               }
                                           }();

                                           if( ar.empty() ) // Fetch failed... so try to create.
                                           {
                                               using T = std::decay_t< decltype( vop ) >;
                                               // if returns arg.empty(), and arg is child, create child.
                                               // elif returns arg.empty(), and arg is desc, create_descendents
                                               if constexpr( std::is_same_v< T, Attr >
                                                          || std::is_same_v< T, Child >
                                                          || std::is_same_v< T, Desc >
                                                          || std::is_same_v< T, DirectDesc >
                                                          || std::is_same_v< T, Tag > )
                                               {
                                                   vrv = UuidSet{ KMAP_TRY( vop.create( foc_op.kmap, node ) ) };
                                               }
                                               else if constexpr( std::is_same_v< T, Alias > )
                                               {
                                                   vrv = KMAP_TRY( vop.create( foc_op.kmap, node ) );
                                               }
                                           }
                                           else
                                           {
                                               // ok, so arg() returned something, so we succeeded, no need to try to create.
                                               vrv = UuidSet{ ar };
                                           }

                                           return vrv;
                                       }
                                       , op );

            if( !r )
            {
                return KMAP_PROPAGATE_FAILURE( r );
            }

            cumulative.insert( r.value().begin(), r.value().end() );
        }

        ns = cumulative;
    }

    // TODO: So, one problem here is in the "create" phase. If we create and end up with multiple created paths, we return an error, but the paths
    //       were still created. Outside of undo, I don't see a good way around this, given the requirements of this function, and that the function
    //       doesn't "know" about success or failure until after creation. I think undo is the only way to make this sound.
    if( ns.size() == 1 )
    {
        rv = *ns.begin();
    }
    else if( ns.size() > 1 )
    {
        rv = KMAP_MAKE_ERROR( error_code::network::ambiguous_path );
        // TODO: undo.... requires nested undo, no?
    }
    else // size == 0
    {
        rv = KMAP_MAKE_ERROR( error_code::network::invalid_node );
        // TODO: undo.... requires nested undo, no?
    }

    return rv;
}

auto operator|( Intermediary const& i
              , Single const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

auto operator|( Intermediary const& i
              , Tag const& op )
    -> Intermediary
{
    auto rv = i;

    rv.op_chain.emplace_back( op );

    return rv;
}

} // kmap::view
