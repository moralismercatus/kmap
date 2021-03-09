/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "tag.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>

#include <string>

using namespace ranges;

namespace kmap::cmd {

auto create_tag( Kmap& kmap )
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

        if( auto const tag_path = fmt::format( "/tags.{}"
                                             , args[ 0 ] )
          ; !kmap.exists( tag_path ) )
        {
            if( auto const tag = kmap.fetch_or_create_leaf( tag_path )
            ; tag )
            {
                KMAP_TRY( kmap.select_node( *tag ) );

                return fmt::format( "added: {}"
                                  , tag_path );
            }
            else
            {
                return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "unable to acquire {}", tag_path ) );
            }
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "tag already exists {}", tag_path ) );
        }
    };
}

auto add_tag( Kmap& kmap )
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
        auto const troot_path = "/tags";
        auto const tag_root = kmap.fetch_leaf( troot_path );

        if( !tag_root )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "unable to acquire {}", troot_path ) );
        }

        auto const tag_path_v = args[ 0 ]
                              | views::split( '.' )
                              | views::transform( []( auto const& e ){ return to< std::string >( e ); } )
                              | to< StringVec >();
        auto const tag_path = tag_path_v
                            | views::reverse
                            | views::join( '.' )
                            | to< std::string >();
        auto const tag = kmap.fetch_leaf( *tag_root
                                        , *tag_root
                                        , ".tags." + tag_path );

        if( !tag )
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "unable to acquire {}", tag_path ) );
        }

        auto alias_parent = [ & ]
        {
            if( kmap.is_child( target
                              , "tags" ) )
            {
                return *kmap.database()
                            .fetch_child( "tags"
                                        , target );
            }
            else
            {
                return kmap.create_child( target
                                        , "tags" ).value();
            }
        }();

        if( auto const alias = kmap.create_alias( *tag
                                                , alias_parent )
          ; alias )
        {
            KMAP_TRY( kmap.select_node( target ) ); // We don't want to move to the newly added alias.

            return fmt::format( "added: {}", tag_path );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "unable to add tag: {}", tag_path ) );
        }
    };
}

} // namespace kmap::cmd