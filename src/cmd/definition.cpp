/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "definition.hpp"

#include "../contract.hpp"
#include "../common.hpp"
#include "../kmap.hpp"
#include "../io.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>

#include <string>

using namespace ranges;

namespace kmap::cmd {

auto create_definition( Kmap& kmap )
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
        auto const def_path = fmt::format( "/definitions.{}"
                                         , format_heading( title ) );
        // TODO: Should this really be a fetch_or_create, or should it fail if it already exists?
        if( auto const def = kmap.fetch_or_create_leaf( def_path )
          ; def )
        {
            KMAP_TRY( kmap.update_title( *def, title ) );
            KMAP_TRY( kmap.select_node( *def ) );

            return fmt::format( "added: {}", def_path );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "unable to acquire {}", def_path ) );
        }
    };
}

// TODO: Decide whether this should create an alias under the common "references" or the specific "definitions".
auto add_definition( Kmap& kmap )
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

        // TODO: Match inverted...
        auto const target = kmap.selected_node();
        auto const droot_path = "/definitions";
        auto const def_root = kmap.fetch_leaf( droot_path );

        if( !def_root )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized , fmt::format( "unable to acquire {}", droot_path ) );
        }

        auto const def_path_v = args[ 0 ]
                              | views::split( '.' )
                              | views::transform( []( auto const& e ){ return to< std::string >( e ); } )
                              | to< StringVec >();
        auto const def_path = def_path_v
                            | views::reverse
                            | views::join( '.' )
                            | to< std::string >();
        auto const def = kmap.fetch_leaf( *def_root
                                        , *def_root
                                        , ".definitions." + def_path );

        if( !def )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "unable to acquire {}", def_path ) );
        }

        auto alias_parent = [ & ]
        {
            if( kmap.is_child( target
                              , "definitions" ) )
            {
                return kmap.fetch_child( target, "definitions" ).value(); // TODO: Handle failure case.
            }
            else
            {
                return kmap.create_child( target, "definitions" ).value(); // TODO: Handle failure case.
            }
        }();

        if( auto const alias = kmap.create_alias( *def, alias_parent )
          ; alias )
        {
            KMAP_TRY( kmap.select_node( target ) ); // We don't want to move to the newly added alias.

            return fmt::format( "added: {}", def_path );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "failed to add: {}", def_path ) );
        }
    };
}

} // namespace kmap::cmd