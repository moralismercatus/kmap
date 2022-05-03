/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_STMT_PREP_HPP
#define KMAP_STMT_PREP_HPP

#include "common.hpp"

#include <map>
#include <set>
#include <utility>

namespace kmap
{

class Kmap;

class StatementPreparer
{
public:
    StatementPreparer() = default;
    virtual ~StatementPreparer() = default;

    auto commit( Kmap& kmap )
        -> void;
    auto commit_nodes( Kmap& kmap )
        -> void;
    auto commit_children( Kmap& kmap )
        -> void;
    auto commit_headings( Kmap& kmap )
        -> void;
    auto commit_titles( Kmap& kmap )
        -> void;
    auto commit_bodies( Kmap& kmap )
        -> void;
    [[ maybe_unused ]]
    auto create_alias( Uuid const& src
                     , Uuid const& dst )
        -> Optional< std::pair< Uuid, Uuid > >;
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Uuid const& child
                     , Title const& title
                     , Heading const& heading )
        -> Uuid;
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Uuid const& child
                     , Heading const& heading )
        -> Uuid;
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Title const& title
                     , Heading const& heading )
        -> Uuid;
    [[ maybe_unused ]]
    auto create_child( Uuid const& parent
                     , Heading const& heading )
        -> Uuid;
    auto erase_node( Uuid const& id )
        -> void;
    [[ nodiscard ]]
    auto exists( Uuid const& id ) const
        -> bool;
    [[ nodiscard ]]
    auto fetch_child( Uuid const& parent
                    , Heading const& heading ) const
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto fetch_children( Uuid const& parent ) const
        -> std::vector< Uuid >;
    [[ nodiscard ]]
    auto fetch_heading( Uuid const& id ) const
        -> Optional< Heading >;
    [[ nodiscard ]]
    auto fetch_parent( Uuid const& id ) const
        -> Optional< Uuid >;
    [[ nodiscard ]]
    auto fetch_title( Uuid const& id ) const
        -> Optional< Title >;
    [[ nodiscard ]]
    auto is_child( Uuid const& parent
                 , Uuid const& id ) const
        -> bool;
    [[ maybe_unused ]]
    auto move_node( Uuid const& src
                  , Uuid const& dst )
        -> std::pair< Uuid
                    , Uuid >;
    auto update_heading( Uuid const& id
                       , Heading const& heading )
        -> void;
    auto update_title( Uuid const& id
                     , Title const& title )
        -> void;
    auto update_body( Uuid const& id
                    , std::string_view const contents )
        -> void;

private:
    using NodesPrep = std::set< Uuid >;
    using ChildrenPrep = std::map< Uuid
                                 , std::vector< Uuid > >;
    using HeadingsPrep = std::map< Uuid
                                 , Heading >;
    using TitlesPrep = std::map< Uuid
                               , Title >;
    using BodiesPrep = std::map< Uuid
                               , std::string >;
    using AliasPair = std::pair< Uuid
                               , Uuid >;
    using SrcTag = struct{};
    using DstTag = struct{};
    using AliasesPrep = bmi::multi_index_container< AliasPair
                                                  , bmi::indexed_by< bmi::hashed_non_unique< bmi::tag< SrcTag >
                                                                                           , bmi::member< AliasPair
                                                                                                        , decltype( AliasPair::first )
                                                                                                        , &AliasPair::first > >
                                                                    , bmi::hashed_non_unique< bmi::tag< DstTag >
                                                                                            , bmi::member< AliasPair
                                                                                                         , decltype( AliasPair::second )
                                                                                                         , &AliasPair::second > > > >;

    NodesPrep nodes_prep_ = {};
    ChildrenPrep children_prep_ = {};
    HeadingsPrep headings_prep_ = {};
    TitlesPrep titles_prep_ = {};
    BodiesPrep bodies_prep_ = {};
    AliasesPrep aliases_prep_ = {};
};

class ScopedStmtPrep : public StatementPreparer
{
public:
    ScopedStmtPrep( Kmap& kmap );
    ~ScopedStmtPrep();

private:
    Kmap& kmap_;
};

} // namespace kmap

#endif // KMAP_STMT_PREP_HPP
