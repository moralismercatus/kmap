/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "stmt_prep.hpp"
#include "kmap.hpp"
#include "contract.hpp"
#include "db.hpp"
#include <sqlpp11/sqlite3/insert_or.h>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/join.hpp>

using namespace ranges;

namespace kmap
{

auto StatementPreparer::commit( Kmap& kmap )
    -> void
{
    commit_nodes( kmap );
    commit_children( kmap );
    commit_headings( kmap );
    commit_titles( kmap );
    commit_bodies( kmap );
}

auto StatementPreparer::commit_nodes( Kmap& kmap )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            for( auto const& n : nodes_prep_ )
            {
                BC_ASSERT( !kmap.exists( n ));
            }
        })
        BC_POST([ & ]
        {
            BC_ASSERT( nodes_prep_.empty() );

            for( auto const& n : nodes_prep_ )
            {
                BC_ASSERT( kmap.exists( n ));
                BC_ASSERT( present_time() >= kmap.fetch_genesis_time( n ) );
            }
        })
    ;

    if( !nodes_prep_.empty() )
    {
        auto& db = kmap.database();

        {
            auto nt = nodes::nodes{};
            auto inserter = insert_into( nt ).columns( nt.uuid );

            for( auto const& n : nodes_prep_ )
            {
                inserter.values.add( nt.uuid = to_string( n ) );
            }

            db.execute( inserter );
        }

        nodes_prep_ = {};
    }
}

auto StatementPreparer::commit_children( Kmap& kmap )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            for( auto const& [ pid, cids ] : children_prep_ )
            {
                BC_ASSERT( kmap.exists( pid ) );
                
                for( auto const& cid : cids )
                {
                    BC_ASSERT( kmap.exists( cid ) );
                    BC_ASSERT( !kmap.is_child( pid
                                             , cid ) );
                }
            }
        })
        BC_POST([ & ]
        {
            BC_ASSERT( children_prep_.empty() );

            for( auto const& [ pid, cids ] : children_prep_ )
            {
                for( auto const& cid : cids )
                {
                    BC_ASSERT( kmap.exists( cid ) );
                    BC_ASSERT( kmap.is_child( pid
                                            , cid ) );
                }
            }
        })
    ;

    if( !children_prep_.empty() )
    {
        auto& db = kmap.database();

        {
            auto ct = children::children{};
            auto inserter = insert_into( ct ).columns( ct.parent_uuid
                                                     , ct.child_uuid );

            for( auto const& [ pid, cids ] : children_prep_ )
            {
                for( auto const& cid : cids )
                {
                    inserter.values.add( ct.parent_uuid = to_string( pid )
                                       , ct.child_uuid = to_string( cid ) );
                }
            }

            db.execute( inserter );
        }
        // {
        //     using sqlpp::sqlite3::insert_or_replace_into;

        //     auto co = child_orderings::child_orderings{};
        //     auto inserter = insert_or_replace_into( co ).columns( co.parent_uuid
        //                                                         , co.sequence );

        //     for( auto const& [ pid, cids ] : children_prep_ )
        //     {
        //         auto const children = fetch_children( pid );
        //         auto const ids = children
        //                        | views::transform( to_ordering_id )
        //                        | to< StringVec >();
        //         auto const ordering = ids
        //                             | views::join
        //                             | to< std::string >();
        //         auto const spid = to_string( pid );

        //         inserter.values.add( co.parent_uuid = spid
        //                            , co.sequence = ordering );
        //     }

        //     db.execute( inserter );
        // }

        children_prep_ = {};
    }
}

auto StatementPreparer::commit_headings( Kmap& kmap )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            for( auto const& [ id, h ] : headings_prep_ )
            {
                BC_ASSERT( kmap.exists( id ) );
                BC_ASSERT( is_valid_heading( h ) );
            }
        })
        BC_POST([ & ]
        {
            for( auto const& [ id, h ] : headings_prep_ )
            {
                BC_ASSERT( kmap.fetch_heading( id )
                               .has_value() );
            }
        })
    ;

    if( !headings_prep_.empty() )
    {
        auto& db = kmap.database();

        {
            using sqlpp::sqlite3::insert_or_replace_into;

            auto ht = headings::headings{};
            auto inserter = insert_or_replace_into( ht ).columns( ht.uuid
                                                                , ht.heading );

            for( auto const& [ id, h ] : headings_prep_ )
            {
                inserter.values.add( ht.uuid = to_string( id )
                                   , ht.heading = h );
            }

            db.execute( inserter );
        }

        headings_prep_ = {};
    }
}

auto StatementPreparer::commit_titles( Kmap& kmap )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            for( auto const& [ id, t ] : titles_prep_ )
            {
                BC_ASSERT( kmap.exists( id ) );
            }
        })
        BC_POST([ & ]
        {
            for( auto const& [ id, t ] : titles_prep_ )
            {
                BC_ASSERT( kmap.fetch_title( id )
                               .has_value() );
            }
        })
    ;

    if( !titles_prep_.empty() )
    {
        auto& db = kmap.database();

        {
            using sqlpp::sqlite3::insert_or_replace_into;

            auto tt = titles::titles{};
            auto inserter = insert_or_replace_into( tt ).columns( tt.uuid
                                                                , tt.title );

            for( auto const& [ id, t ] : titles_prep_ )
            {
                inserter.values.add( tt.uuid = to_string( id )
                                   , tt.title = t );
            }

            db.execute( inserter );
        }

        titles_prep_ = {};
    }
}

auto StatementPreparer::commit_bodies( Kmap& kmap )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            for( auto const& [ id, t ] : bodies_prep_ )
            {
                BC_ASSERT( kmap.exists( id ) );
            }
        })
        BC_POST([ & ]
        {
            for( auto const& [ id, b ] : bodies_prep_ )
            {
                BC_ASSERT( kmap.fetch_body( id )
                               .has_value() );
            }
        })
    ;

    if( !bodies_prep_.empty() )
    {
        auto& db = kmap.database();

        {
            using sqlpp::sqlite3::insert_or_replace_into;

            auto bt = bodies::bodies{};
            auto inserter = insert_or_replace_into( bt ).columns( bt.uuid
                                                                , bt.body );

            for( auto const& [ id, b ] : bodies_prep_ )
            {
                inserter.values.add( bt.uuid = to_string( id )
                                   , bt.body = b );
            }

            db.execute( inserter );
        }

        bodies_prep_ = {};
    }
}

auto StatementPreparer::create_alias( Uuid const& src
                                    , Uuid const& dst )
    -> Optional< AliasPair >
{
    auto rv = Optional< AliasPair >{};
    auto const p = AliasPair{ src, dst };

    // TODO: Should fail if alias already exists
    //if( aliases_prep_.count( p ) == 0 )
    {
        aliases_prep_.emplace( p );

        rv = p;
    }

    return rv;
}

auto StatementPreparer::create_child( Uuid const& parent
                                    , Uuid const& child
                                    , Title const& title
                                    , Heading const& heading )
    -> Uuid
{
    nodes_prep_.emplace( child );
    children_prep_[ parent ].emplace_back( child );
    headings_prep_.emplace( child
                          , heading );
    titles_prep_.emplace( child
                        , title );
    bodies_prep_.emplace( child
                        , "" );

    return child;
}

auto StatementPreparer::create_child( Uuid const& parent
                                    , Uuid const& child
                                    , Heading const& heading )
    -> Uuid
{
    return create_child( parent
                       , child
                       , heading
                       , heading );
}

auto StatementPreparer::create_child( Uuid const& parent
                                    , Title const& title
                                    , Heading const& heading )
    -> Uuid
{
    auto rv = create_child( parent
                          , gen_uuid()
                          , title
                          , heading );

    return rv;
}

auto StatementPreparer::create_child( Uuid const& parent
                                    , Heading const& heading )
    -> Uuid
{
    auto rv = create_child( parent
                          , format_title( heading )
                          , heading );

    return rv;
}

auto StatementPreparer::erase_node( Uuid const& id )
    -> void 
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !exists( id ) );
        })
    ;

    bodies_prep_.erase( id );
    titles_prep_.erase( id );
    headings_prep_.erase( id );
    children_prep_.erase( id );
    nodes_prep_.erase( id );
}

auto StatementPreparer::exists( Uuid const& id ) const
    -> bool
{
    return nodes_prep_.count( id ) != 0;
}

auto StatementPreparer::fetch_children( Uuid const& parent ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            auto rv_set = rv | to< std::set< Uuid > >();
            BC_ASSERT( rv.size() == rv_set.size() );
        })
    ;

    if( auto const it = children_prep_.find( parent )
      ; it != children_prep_.end() )
    {
        rv = it->second;
    }

    return rv;
}

auto StatementPreparer::fetch_child( Uuid const& parent
                                   , Heading const& heading ) const
        -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};
    auto const children = fetch_children( parent );

    for( auto const& c : children )
    {
        if( auto const h = fetch_heading( c )
          ; h )
        {
            if( heading == *h )
            {
                rv = c;

                break;
            }
        }
    }

    return rv;
}

auto StatementPreparer::fetch_heading( Uuid const& id ) const
        -> Optional< Heading >
{
    // TODO: constraints on local state
    auto rv = Optional< Heading >{};

    if( auto const& it = headings_prep_.find( id )
      ; it != headings_prep_.end() )
    {
        rv = it->second;
    }

    return rv;
}

auto StatementPreparer::fetch_parent( Uuid const& id ) const
        -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    for( auto const& [ pid, cids ] : children_prep_ )
    {
        if( auto const it = find( cids
                                , id )
          ; it != cids.end() )
        {
            rv = pid;

            break;
        }
    }

    return rv;
}

auto StatementPreparer::fetch_title( Uuid const& id ) const
        -> Optional< Title >
{
    auto rv = Optional< Title >{};

    if( auto const it = titles_prep_.find( id )
      ; it != titles_prep_.end() )
    {
        rv = it->second;
    }

    return rv;
}
auto StatementPreparer::is_child( Uuid const& parent
                                , Uuid const& id ) const
    -> bool
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
            BC_ASSERT( exists( id ) );
        })
    ;

    for( auto const& c : fetch_children( id ) )
    {
        if( c == id )
        {
            return true;
        }
    }

    return false;
}

auto StatementPreparer::move_node( Uuid const& src
                                 , Uuid const& dst )
    -> std::pair< Uuid
                , Uuid >
{
    auto rv = std::pair{ dst
                       , src };
    auto const src_parent = fetch_parent( src );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( src ) );
            BC_ASSERT( exists( dst ) );
            BC_ASSERT( src_parent.has_value() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( is_child( dst
                               , src ) );
            BC_ASSERT( !is_child( *src_parent
                                , src ) );
        })
    ;

    if( !is_child( dst
                 , src ) )
    {
        auto& src_children = children_prep_.find( *src_parent )
                                          ->second;
        auto const src_it = find( src_children
                                , src );
        
        src_children.erase( src_it );

        auto& children = children_prep_[ dst ];

        children.emplace_back( src );

        {
            auto cset = children | to< std::set< Uuid > >();
            assert( children.size() == cset.size() );
        }
    }

    return rv;
}

auto StatementPreparer::update_body( Uuid const& id 
                                   , std::string_view const content )
    -> void
{
    if( auto const it = bodies_prep_.find( id )
      ; it != bodies_prep_.end() )
    {
        it->second = content;
    }
}

auto StatementPreparer::update_heading( Uuid const& id 
                                      , Heading const& heading )
    -> void
{
    if( auto const it = headings_prep_.find( id )
      ; it != headings_prep_.end() )
    {
        it->second = heading;
    }
}

auto StatementPreparer::update_title( Uuid const& id
                                    , Title const& title )
    -> void
{
    if( auto const it = titles_prep_.find( id )
      ; it != titles_prep_.end() )
    {
        it->second = title;
    }
}

ScopedStmtPrep::ScopedStmtPrep( Kmap& kmap )
    : kmap_{ kmap }
{
}

ScopedStmtPrep::~ScopedStmtPrep()
{
    commit( kmap_ );
}

} // namespace kmap


