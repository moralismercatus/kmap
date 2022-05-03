/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "recipe.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"

namespace kmap::cmd {

namespace {

auto fetch_recipes_root( Kmap const& kmap )
    -> Result< Uuid >
{
    return KMAP_TRY( kmap.fetch_descendant( "/recipes" ) );
}

auto is_recipe( Kmap const& kmap
              , Uuid const& id )
    -> bool
{
    auto rv = false;

    if( auto const recipes_root = fetch_recipes_root( kmap )
      ; recipes_root )
    {
        rv = kmap.is_lineal( recipes_root.value(), id )
          && has_geometry( kmap
                         , id
                         , std::regex{ "step|steps" } );
    }

    return rv;
}

auto fetch_recipe_root( Kmap const& kmap
                      , Uuid const& node )
    -> Result< Uuid >
{
    auto const recipes_root = KMAP_TRY( fetch_recipes_root( kmap ) );

    auto const rv = fetch_nearest_ascending( kmap
                                  , recipes_root
                                  , node
                                  , is_recipe );
    return rv;
}

auto is_in_recipe_tree( Kmap const& kmap
                      , Uuid const& node )
    -> bool
{
    return fetch_recipe_root( kmap, node ).has_value();
}

// TODO: Redundant functionality with project, conclusion.
// TODO: Replace with fetch_recipe_root? Or replace with fetch_recipe_root and rename to fetch_parent_recipe?
auto fetch_parent_recipe( Kmap const& kmap
                        , Uuid const& id )
    -> Result< Uuid > 
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( id ) );
        })
    ;

    auto rv = KMAP_MAKE_RESULT( Uuid );

    auto const recipe_root = KMAP_TRY( fetch_recipes_root( kmap ) );
    rv = kmap.fetch_nearest_ancestor( recipe_root
                                    , id
                                    , std::regex{ "step|steps" } );

    return rv;
}

auto fetch_nearest_category( Kmap const& kmap
                           , Uuid const& id )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid ); 

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( id ) );
        })
    ;

    auto const recipes_root = KMAP_TRY( fetch_recipes_root( kmap ) );

    rv = fetch_nearest_ascending( kmap
                                , recipes_root
                                , id
                                , std::not_fn( is_in_recipe_tree ) );

    if( !rv )
    {
        rv = recipes_root;
    }

    return rv;
}

auto create_recipe( Kmap& kmap
                  , Heading const& heading )
    -> Result< Uuid > 
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const selected = kmap.selected_node();
    auto const cat = KMAP_TRY( fetch_nearest_category( kmap, selected ) );
    auto const cid = KMAP_TRY( kmap.create_child( cat, heading ) );

    KMAP_TRY( kmap.create_child( cid, "step" ) );

    rv = cid;

    return rv;
}

auto convert_to_multistep_recipe( Kmap& kmap
                                , Uuid const& recipe )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( kmap.exists( recipe ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( kmap.exists( rv.value() ) );
            }
        })
    ;

    if( auto const single_step = kmap.fetch_child( recipe
                                                 , "step" )
      ; single_step )
    {
        if( auto const body = kmap.fetch_body( single_step.value() )
          ; body
         && body.value().empty() )
        {
            KMAP_TRY( kmap.erase_node( single_step.value() ) );
            KMAP_TRY( kmap.create_child( recipe, "steps" ) );
        }
    }

    rv = kmap.fetch_child( recipe, "steps" );

    return rv;
}

auto create_step( Kmap& kmap
                , Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid ); 

    auto const pr = KMAP_TRY( fetch_parent_recipe( kmap, kmap.selected_node() ) );
    auto const steps = KMAP_TRY( convert_to_multistep_recipe( kmap, pr ) );
    auto const ns = KMAP_TRY( create_recipe( kmap, heading ) );
    rv = KMAP_TRY( kmap.create_alias( ns, steps ) );

    return rv;
}

auto add_step( Kmap& kmap
             , Heading const& heading )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const pr = fetch_parent_recipe( kmap
                                           , kmap.selected_node() )
      ; pr )
    {
        if( auto const steps = convert_to_multistep_recipe( kmap
                                                          , pr.value() )
          ; steps )
        {
            if( auto const recipes_root = fetch_recipes_root( kmap )
              ; recipes_root )
            {
                if( auto const ns = kmap.fetch_descendant( recipes_root.value()
                                                         , heading )
                  ; ns )
                {
                    if( auto const aid = kmap.create_alias( ns.value()
                                                          , steps.value() )
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

auto add_prerequisite( Kmap& kmap
                     , Heading const& heading )
    -> Optional< Uuid > // TODO: Should be some type that can return reason for failure e.g., Boost.Outcome
{
    auto rv = Optional< Uuid >{};

    if( auto const pr = fetch_parent_recipe( kmap
                                           , kmap.selected_node() )
      ; pr )
    {
        if( auto const prereq_root = kmap.fetch_or_create_descendant( pr.value()
                                                                     , "/prerequisites" )
          ; prereq_root )
        {
            if( auto const recipes_root = fetch_recipes_root( kmap )
              ; recipes_root )
            {
                if( auto const np = kmap.fetch_descendant( recipes_root.value()
                                                         , heading )
                  ; np )
                {
                    if( auto const aid = kmap.create_alias( np.value()
                                                          , prereq_root.value() )
                      ; aid )
                    {
                        rv = aid.value();
                    }
                }
            }
        }

        BOOST_TEST( kmap.select_node( pr.value() ) );
    }

    return rv;
}

} // anonymous ns

auto create_recipe( Kmap& kmap )
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
        auto const root = kmap.fetch_or_create_descendant( "/recipes" );

        // TODO: I don't believe this should ever fail, except in circumstances
        // that would throw an exception; therefore, replace assert with throw.
        BC_ASSERT( root );

        if( auto const cid = create_recipe( kmap, heading )
          ; cid )
        {
            KMAP_TRY( kmap.update_title( cid.value(), title ) );
            KMAP_TRY( kmap.select_node( cid.value() ) );
      
            return fmt::format( "{} added to {}"
                              , heading
                              , "/recipes" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "recipe heading already exists" ) );
        }
    };
}

auto create_step( Kmap& kmap )
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
                // TODO: Assert that if successful, created recipe/step is unique.
            })
        ;

        auto const title = flatten( args, ' ' );
        auto const heading = format_heading( title );

        if( auto const step = create_step( kmap
                                         , heading )
          ; step )
        {
            KMAP_TRY( kmap.update_title( step.value(), title ) );
            KMAP_TRY( kmap.select_node( step.value() ) );

            return fmt::format( "created recipe step" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to create recipe step" ) );
        }
    };
}

// TODO: Need to inform user reason for failure e.g., that the "step" is nonempty.
// TODO: Needs to ensure the step to be added isn't the recipe itself!
auto add_step( Kmap& kmap )
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

        if( auto const step = add_step( kmap
                                      , heading )
          ; step )
        {
            return fmt::format( "added recipe step" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to add recipe step" ) );
        }
    };
}

// TODO: Needs to ensure the step to be added isn't the recipe itself!
auto add_prerequisite( Kmap& kmap )
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

        if( auto const aid = add_prerequisite( kmap
                                             , heading )
          ; aid )
        {
            return fmt::format( "added recipe prerequisite" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to add recipe prerequisite" ) );
        }
    };
}

auto create_prerequisite( Kmap& kmap )
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


        if( auto const nr = create_recipe( kmap
                                         , heading )
          ; nr )
        {
            if( auto const aid = add_prerequisite( kmap
                                                 , heading )
              ; aid )
            {
                return fmt::format( "added recipe prerequisite" );
            }
            else
            {
                return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to add recipe prerequisite" ) );
            }
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to create prerequisite recipe" ) );
        }
    };
}

} // namespace kmap::cmd