/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_HPP
#define KMAP_PATH_HPP

#include "common.hpp"
#include "kmap.hpp"
#include "util/result.hpp"

#include <regex>
#include <set>

namespace kmap::com
{
    class Network;
}

namespace kmap {

class Kmap;
class Lineal;
class LinealRange;

struct CompletionNode
{
    Uuid target;
    std::string path;
    UuidPath disambig;
    // TODO: last state e.g., variant< sm::ev::DisNode, etc. >?
};

auto operator<( CompletionNode const& lhs
              , CompletionNode const& rhs ) 
    -> bool;

using CompletionNodeSet = std::set< CompletionNode >;

auto absolute_path( Kmap const& kmap
                  , Uuid const& desc )
    -> Result< UuidVec >;
auto absolute_path_flat( Kmap const& km
                       , Uuid const& node )
    -> Result< std::string >;
auto complete_any( Kmap const& kmap
                 , Uuid const& root
                 , std::string const& heading )
    -> Result< UuidSet >;
[[ nodiscard ]]
auto complete_child_heading( Kmap const& kmap
                           , Uuid const& root_id
                           , Heading const& raw )
    -> IdHeadingSet;
[[ nodiscard ]]
auto complete_path( Kmap const& kmap
                  , Uuid const& root_id
                  , Uuid const& selected_node
                  , std::string const& raw )
    -> Result< CompletionNodeSet >;
[[ nodiscard ]]
auto complete_path_reducing( Kmap const& kmap
                           , Uuid const& root
                           , Uuid const& selected
                           , std::string const& path )
    -> Result< CompletionNodeSet >;
[[ nodiscard ]]
auto complete_selection( Kmap const& kmap
                       , Uuid const& root
                       , Uuid const& selected
                       , std::string const& path )
    -> Result< CompletionNodeSet >;
[[ nodiscard ]]
auto disambiguate_paths( Kmap const& kmap
                       , std::map< Uuid, std::string > const& nodes )
    -> Result< std::map< Uuid, std::string > >;
[[ nodiscard ]]
auto disambiguate_path( Kmap const& km
                      , Uuid const& node )
    -> Result< std::map< Uuid, Uuid > >;
[[ nodiscard ]]
auto disambiguate_paths2( Kmap const& km
                        , std::vector< std::vector< Uuid > > const& lineages )
    -> Result< std::map< Uuid, Uuid > >;
[[ nodiscard ]]
auto decide_path( Kmap const& kmap 
                , Uuid const& root
                , Uuid const& selected
                , StringVec const& tokens )
    -> Result< UuidVec >;
[[ nodiscard ]]
auto decide_path( Kmap const& kmap 
                , Uuid const& root
                , Uuid const& selected
                , std::string const& raw )
    -> Result< UuidVec >;
[[ nodiscard ]]
auto fetch_descendants( Kmap const& kmap 
                      , Uuid const& root_id
                      , Uuid const& selected
                      , std::string const& raw )
    -> Result< UuidVec >;
template< typename KMap
        , typename Pred >
[[ nodiscard ]]
auto fetch_descendants( Kmap const& kmap 
                      , Uuid const& root
                      , Pred pred )
    -> Result< UuidSet >;
[[ nodiscard ]]
auto fetch_descendant( Kmap const& kmap
                     , Uuid const& root
                     , Uuid const& selected
                     , Heading const& descendant_path )
    -> Result< Uuid >;
// Note: There doesn't seem to be enough to justify this function while fetch_nearest_ancestor exists, such that the geometry could be supplied as a predicate to it.
// Inclusive of "leaf". Exclusive of "root".
[[ nodiscard ]]
auto fetch_nearest_ancestor( Kmap const& kmap
                           , Uuid const& root
                           , Uuid const& leaf
                           , std::regex const& geometry )
    -> Result< Uuid >;
[[ nodiscard ]]
auto fetch_nearest_ascending( Kmap const& kmap
                            , Uuid const& root 
                            , Uuid const& leaf 
                            , std::function< bool( Kmap const&, Uuid const& ) > pred )
    -> Result< Uuid >;
[[ nodiscard ]]
auto has_geometry( Kmap const& kmap
                 , Uuid const& parent 
                 , std::regex const& geometry )
    -> bool;
[[ nodiscard ]]
auto is_absolute( Heading const& heading )
    -> bool;
[[ nodiscard ]]
auto is_ancestor( com::Network const& nw
                , Uuid const& ancestor
                , Uuid const& descendant )
    -> bool;
[[ nodiscard ]]
auto is_leaf( Kmap const& kmap
            , Uuid const& node )
    -> bool;
// TODO: Move to Network.hpp
[[ nodiscard ]]
auto is_lineal( com::Network const& nw
              , Uuid const& ancestor
              , Uuid const& descendant )
    -> bool;
[[ nodiscard ]]
auto is_lineal( com::Network const& nw
              , Uuid const& ancestor
              , Heading const& descendant )
    -> bool;
[[ nodiscard ]]
auto is_ascending( Kmap const& kmap
                 , UuidVec const& lineage )
    -> bool;
[[ nodiscard ]]
auto is_descending( Kmap const& kmap
                  , UuidVec const& lineage )
    -> bool;
[[ nodiscard ]]
auto is_parent( com::Network const& nw
              , Uuid const& parent
              , Uuid const& child )
    -> bool;
[[ nodiscard ]]
auto is_sibling_adjacent( Kmap const& km
                        , Uuid const& node
                        , Uuid const& other )
    -> bool;
[[ nodiscard ]]
auto create_descendants( Kmap& kmap
                       , Uuid const& root
                       , Uuid const& selected
                       , Heading const& heading )
    -> Result< UuidVec >;
[[ nodiscard ]]
auto fetch_or_create_descendant( Kmap& kmap
                               , Uuid const& root
                               , Uuid const& selected
                               , Heading const& heading )
    -> Result< Uuid >;
[[nodiscard]]
auto format_node_label( Kmap const& km
                      , Uuid const& node )
    -> Result< std::string >;
[[ nodiscard ]]
auto tokenize_path( std::string const& path )
    -> StringVec;
[[ nodiscard ]]
auto mirror( Kmap& kmap
           , Lineal const& src
           , Uuid const& dst
           , std::function< Result< void >( Uuid const& src, Uuid const& dst ) > fn )
    -> Result< Uuid >;
[[ nodiscard ]]
auto mirror_basic( Kmap& kmap
                 , LinealRange const& src
                 , Uuid const& dst )
    -> Result< Uuid >;

template< typename KMap
        , typename Pred >
[[ nodiscard ]]
auto fetch_descendants( KMap const& kmap 
                      , Uuid const& root
                      , Pred pred )
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );

    auto rv = kmap::result::make_result< UuidSet >();
    auto const nw = KTRY( kmap.template fetch_component< com::Network >() );
    auto matches = UuidSet{};

    if( pred( root ) )
    {
        matches.emplace( root );
    }

    for( auto const& child : nw->fetch_children( root ) )
    {
        auto const descs = KTRY( fetch_descendants( kmap, child, pred ) );

        matches.insert( descs.begin(), descs.end() );
    }

    rv = matches;

    return rv;
}

/// Returns all descendants of 'root'.
auto fetch_descendants( Kmap const& kmap
                      , Uuid const& root )
    -> Result< UuidSet >;

#if 0
class Heading
{
public:
    Heading() = default;
    Heading( Heading const& ) = default;
    Heading( char const* heading );
    Heading( std::string_view heading );

    auto operator=( Heading const& rhs );
    auto operator=( std::string_view const rhs );

    auto begin()
        -> std::string::iterator;
    auto begin() const
        -> std::string::const_iterator;
    auto end()
        -> std::string::iterator;
    auto end() const
        -> std::string::const_iterator;
    auto cbegin() const
        -> std::string::const_iterator;
    auto cend() const
        -> std::string::const_iterator;

private:
    std::string str_ = {};
};

class HeadingPath
{
public:
    using Headings = std::vector< Heading >;

    HeadingPath() = default;
    // HeadingPath( char const* heading );
    HeadingPath( std::string_view const path );
    HeadingPath( Heading const& path );

    auto begin()
        -> Headings::iterator;
    auto begin() const
        -> Headings::const_iterator;
    auto end()
        -> Headings::iterator;
    auto end() const
        -> Headings::const_iterator;
    auto cend() const
        -> Headings::const_iterator;
    auto cbegin() const
        -> Headings::const_iterator;

private:
    Headings headings_ = {}; 
};

auto operator/( HeadingPath const& lhs
              , HeadingPath const& rhs )
    -> HeadingPath;
#endif // 0


} // namespace kmap

#endif // KMAP_PATH_HPP
