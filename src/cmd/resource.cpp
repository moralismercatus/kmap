/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "resource.hpp"

#include "com/alias/alias.hpp"
#include "com/database/db.hpp"
#include "com/filesystem/filesystem.hpp"
#include "com/network/network.hpp"
#include "common.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "io.hpp"
#include "kmap.hpp"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fs = boost::filesystem;

namespace kmap::cmd {

auto add_resource( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 || args.size() == 2 );
            })
        ;

        auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
        auto const& source = [ & ]
        {
            if( args.size() == 1 )
            {
                return kmap.fetch_leaf( args[ 0 ] );
            }
            else
            {
                return Optional< Uuid >{};
            }
        }();

        if( !source )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "resource reference node not found: {}", args[ 0 ] ) );
        }

        auto const& target = [ & ]() -> Optional< Uuid >
        {
            if( args.size() == 2 )
            {
                auto const rv = kmap.root_view()
                              | view::desc( args[ 1] )
                              | view::fetch_node( kmap );
                
                return rv ? rv.value() : Uuid{};
            }
            else
            {
                return { nw->selected_node() };
            }
        }();

        if( !target )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "target node not found" ) );
        }
        else
        {
            auto ref_parent = [ & ]
            {
                if( nw->is_child( *target, "resources" ) )
                {
                    auto const db = KTRYE( kmap.fetch_component< com::Database >() );

                    return db->fetch_child( *target, "resources" ).value();
                }
                else
                {
                    return KTRYE( nw->create_child( *target, "resources" ) );
                }
            }();

            if( auto const alias = nw->create_alias( *source, ref_parent )
              ; alias )
            {
                KTRY( kmap.select_node( *target ) ); // We don't want to move to the newly added alias.

                return "resource reference added";
            }
            else
            {
                return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, "failed to add resource reference" );
            }
        }
    };
}

/// TODO: Use mmap instead of reading from file into local buffer.
auto store_resource( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const& p = com::kmap_root_dir / FsPath{ args[ 0 ] };

        if( !file_exists( p ) )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "file not found: {}", p.string() ) );
        }

        auto const fsize = fs::file_size( p );
        auto data = std::vector< char >{};
        auto ifs = fs::ifstream{ p
                               , std::ios::in | std::ios::binary };

        if( fsize == 0 )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "file empty: {}", p.string() ) );
        }
        if( !ifs.good() )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "unable to open file: {}", p.string() ) );
        }

        data.resize( fsize );
        ifs.read( data.data()
                , fsize );

        auto const resources = kmap.fetch_or_create_leaf( "/resources" );

        if( !resources )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "unable to acquire .root.resources" ) );
        }

        auto const heading = [ & ]
        {
            // if( arg.size() ... ) 
            // else
            {
                return url_to_heading( p.filename().string() );
            }
        }();

        if( nw->is_child( *resources, heading ) )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "{} already exists", heading ) );
        }

        auto const id = kmap.store_resource( *resources
                                           , heading
                                           , reinterpret_cast< std::byte const* >( data.data() )
                                           , data.size() );
        
        if( id )
        {
            return fmt::format( "stored: {}"
                              , p.string() );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "store failed: {}", p.string() ) );
        }
    };
}

auto store_url_resource( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const url = args[ 0 ];
        auto const resource_root = kmap.fetch_or_create_leaf( "/resources.url" );

        if( !resource_root )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "unable to acquire .root.resources.url" ) );
        }
        // TODO: Check that url doesn't already exist, as a resource, in the db.
        auto const id = kmap.store_resource( *resource_root
                                           , url 
                                           , reinterpret_cast< std::byte const* >( url.data() )
                                           , url.size() );
        if( id )
        {
            return fmt::format( "stored: {}", url );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "stor failed: {}", url ) );
        }
    };
}

} // namespace kmap::cmd