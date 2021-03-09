/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "conclusion.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"

namespace kmap::cmd {

// TODO: Abstract functionality. Much of the functionality herein is redundant among conclusion, recipe, and project commands. Recipe and conclusion are almost identical, whereas project is a little different. 

namespace {

auto is_conclusion( Kmap const& kmap
                  , Uuid const& id )
    -> bool
{
    return has_geometry( kmap
                       , id
                       , std::regex{ "assertion|premises" } );
}

auto fetch_parent_conclusion( Kmap& kmap
                            , Uuid const& id )
    -> Optional< Uuid > 
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( id ) );
        })
    ;

    auto const con_root = kmap.fetch_leaf( "/conclusions" );

    if( !con_root )
    {
        return nullopt;
    }

    auto const ancestor = kmap.fetch_nearest_ancestor( *con_root
                                                     , id
                                                     , std::regex{ "assertion|premises" } );

    if( ancestor )
    {
        return { ancestor.value() };
    }
    else
    {
        return nullopt;
    }
}

auto fetch_nearest_category( Kmap const& kmap
                           , Uuid const& id )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( id ) );
        })
    ;

    if( auto const con_root = kmap.fetch_leaf( "/conclusions" )
      ; con_root )
    {
        auto const pred = [ & ]( Kmap const& kmap 
                               , Uuid const& id )
        {
            if( is_conclusion( kmap
                             , id ) )
            {
                return false;
            }
            if( fetch_nearest_ascending( kmap
                                       , *con_root
                                       , id
                                       , is_conclusion ) )
            {
                return false;
            }
            
            return true;
        };

        auto const nearest = fetch_nearest_ascending( kmap
                                                    , *con_root
                                                    , id
                                                    , pred );
        if( !nearest )
        {
            rv = con_root.value();
        }
    }

    return rv;
}

auto create_conclusion( Kmap& kmap
                      , Heading const& heading )
    -> Result< Uuid > 
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const root = [ & ]
    {
        auto const selected = kmap.selected_node();
        auto const con_root = kmap.fetch_or_create_leaf( "/conclusions" );
        auto rv = Optional< Uuid >{ con_root };

        BC_ASSERT( con_root );
        
        if( kmap.is_lineal( *con_root, selected ) )
        {
            if( auto const cat = fetch_nearest_category( kmap, selected )
              ; cat )
            {
                rv = cat;
            }
        }

        return rv;
    }();

    // TODO: I don't believe this should ever fail, except in circumstances
    // that would throw an exception; therefore, replace assert with throw.
    BC_ASSERT( root );

    if( kmap.is_child( *root, heading ) )
    {
        auto const cid = KMAP_TRY( kmap.create_child( *root, heading ) );

        KMAP_TRY( kmap.create_child( cid, "assertion" ) );

        rv = cid;
    }

    return rv;
}

auto convert_to_premised_conclusion( Kmap& kmap
                                   , Uuid const& con )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( con ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( kmap.exists( rv.value() ) );
            }
        })
    ;

    if( auto const assertion = kmap.fetch_child( con
                                               , "assertion" )
      ; assertion )
    {
        if( auto const body = kmap.fetch_body( assertion.value() )
          ; body
         && body.value().empty() )
        {
            KMAP_TRY( kmap.delete_node( assertion.value() ) );
            KMAP_TRY( kmap.create_child( con 
                                       , "premises" ) );
        }
    }

    rv = kmap.fetch_child( con 
                         , "premises" );

    return rv;
}

auto create_premise( Kmap& kmap
                   , Heading const& heading )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const pr = fetch_parent_conclusion( kmap, kmap.selected_node() )
      ; pr )
    {
        if( auto const premises = convert_to_premised_conclusion( kmap, *pr )
          ; premises )
        {
            if( auto const ns = create_conclusion( kmap, heading )
              ; ns )
            {
                if( auto const aid = kmap.create_alias( ns.value()
                                                      , premises.value() )
                  ; aid )
                {
                    rv = aid.value();
                }
            }
        }
    }

    return rv;
}

auto add_premise( Kmap& kmap
                , Heading const& heading )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const pr = fetch_parent_conclusion( kmap
                                               , kmap.selected_node() )
      ; pr )
    {
        if( auto const premises = convert_to_premised_conclusion( kmap
                                                                , *pr )
          ; premises )
        {
            if( auto const con_root = kmap.fetch_leaf( "/conclusions" )
              ; con_root )
            {
                if( auto const ns = kmap.fetch_leaf( *con_root
                                                   , *con_root
                                                   , heading )
                  ; ns )
                {
                    if( auto const aid = kmap.create_alias( *ns
                                                          , premises.value() )
                    ; aid )
                    {
                        rv = aid.value();
                    }
                }
            }
        }
    }

    return rv;
}

auto create_objection( Kmap& kmap
                     , Heading const& heading )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const pr = fetch_parent_conclusion( kmap
                                               , kmap.selected_node() )
      ; pr )
    {
        if( auto const objections = kmap.fetch_or_create_descendant( *pr
                                                                   , "objections" )
          ; objections )
        {
            if( auto const ns = create_conclusion( kmap
                                                 , heading )
              ; ns )
            {
                if( auto const aid = kmap.create_alias( ns.value()
                                                      , objections.value() )
                  ; aid )
                {
                    rv = aid.value();
                }
            }
        }
    }

    return rv;
}

auto add_objection( Kmap& kmap
                  , Heading const& heading )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const pr = fetch_parent_conclusion( kmap
                                               , kmap.selected_node() )
      ; pr )
    {
        if( auto const objections = kmap.fetch_or_create_descendant( *pr
                                                                   , "objections" )
          ; objections )
        {
            if( auto const con_root = kmap.fetch_leaf( "/conclusions" )
              ; con_root )
            {
                if( auto const ns = kmap.fetch_leaf( *con_root
                                                   , *con_root
                                                   , heading )
                  ; ns )
                {
                    if( auto const aid = kmap.create_alias( *ns
                                                          , objections.value() )
                    ; aid )
                    {
                        rv = aid.value();
                    }
                }
            }
        }
    }

    return rv;
}

} // anonymous ns

auto create_conclusion( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() >= 1 );
            })
        ;

        auto const title = flatten( args, ' ' );
        auto const heading = format_heading( title );
        auto const root = kmap.fetch_or_create_leaf( "/conclusions" );

        // TODO: I don't believe this should ever fail, except in circumstances
        // that would throw an exception; therefore, replace assert with throw.
        BC_ASSERT( root );

        if( auto const cid = create_conclusion( kmap, heading )
          ; cid )
        {
            kmap.update_title( cid.value(), title );
            KMAP_TRY( kmap.select_node( cid.value() ) );
      
            return fmt::format( "{} added to {}"
                              , heading
                              , "/conclusions" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "conclusion heading already exists" ) );
        }
    };
}

auto create_premise( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() >= 1 );
            })
            BC_POST([ & ]
            {
                // TODO: Assert that if successful, created conclusion/premise is unique.
            })
        ;

        auto const title = flatten( args, ' ' );
        auto const heading = format_heading( title );

        if( auto const premise = create_premise( kmap
                                               , heading )
          ; premise )
        {
            kmap.update_title( *premise
                             , title );
            KMAP_TRY( kmap.select_node( *premise ) ); 

            return fmt::format( "created conclusion premise" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to create conclusion premise" ) );
        }
    };
}

// TODO: Needs to ensure the premise to be added isn't the project itself!
// TODO: This has a lot of overlap with project, recipe.
auto add_premise( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const heading = args[ 0 ];

        if( auto const premise = add_premise( kmap
                                            , heading )
          ; premise )
        {
            return fmt::format( "added conclusion premise" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to add conclusion premise" ) );
        }
    };
}

auto create_objection( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() >= 1 );
            })
            BC_POST([ & ]
            {
                // TODO: Assert that if successful, created conclusion/objection is unique.
            })
        ;

        auto const title = flatten( args, ' ' );
        auto const heading = format_heading( title );

        if( auto const objection = create_objection( kmap
                                                   , heading )
          ; objection )
        {
            kmap.update_title( *objection
                             , title );
            KMAP_TRY( kmap.select_node( *objection ) ); 

            return fmt::format( "created conclusion objection" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to create conclusion objection" ) );
        }
    };
}

// TODO: Needs to ensure the objection to be added isn't the project itself!
// TODO: This has a lot of overlap with project, recipe.
auto add_objection( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const heading = args[ 0 ];

        if( auto const objection = add_objection( kmap
                                                , heading )
          ; objection )
        {
            return fmt::format( "added conclusion objection" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to add conclusion objection" ) );
        }
    };
}

auto create_citation( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const concl_root = kmap.fetch_leaf( "/conclusions" );

        if( !concl_root )
        {
            KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "citation not found" ) );
        }

        auto const source = kmap.fetch_leaf( *concl_root
                                           , *concl_root
                                           , args[ 0 ] );
        auto const target = kmap.selected_node();

        if( !source )
        {
            KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "citation not found" ) );
        }

        auto ref_parent = [ & ]
        {
            if( kmap.is_child( target
                             , "citations" ) )
            {
                return *kmap.database()
                            .fetch_child( "citations"
                                        , target );
            }
            else
            {
                return kmap.create_child( target
                                        , "citations" ).value();
            }
        }();

        if( auto const alias = kmap.create_alias( *source 
                                                , ref_parent )
          ; alias )
        {
            kmap.select_node( target ); // We don't want to move to the newly added reference.

            return "citation added";
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, "failed to add citation" ); // TODO: Better diagnostics via Boost.Outcome over Optional?
        }
    };
}

} // namespace kmap::cmd