/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "state.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

namespace kmap::cmd {

auto copy_state( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;
        
        using boost::system::error_code;

        auto const& src = kmap.database()
                              .path();
        auto const& dst = FsPath{ args[ 0 ] };
        auto const succ = kmap.copy_state( dst );

        if( !succ )
        {
            return { CliResultCode::failure
                   , fmt::format( "copy failed: {}"
                                , dst.string() ) };
        }

        return { CliResultCode::success
               , fmt::format( "{} copied to {}"
                            , src.string() 
                            , dst.string() ) };
    };
}

auto load_state( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const p = FsPath{ args[ 0 ] };
        auto const succ = kmap.load_state( p );

        if( !succ )
        {
            return { CliResultCode::failure
                   , fmt::format( "failed to load state: {}"
                                , p.string() ) };
        }
        else
        {
            return { CliResultCode::success
                   , fmt::format( "loaded state: {}"
                                , p.string() ) };
        }
    };
}

auto save_state( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;

        auto const p = FsPath{ args[ 0 ] };
        if( auto const succ = kmap.copy_state( p )
                           && kmap.load_state( p )
          ; succ )
        {
            return { CliResultCode::success
                   , fmt::format( "state saved and loaded: {}"
                                , p.string() ) };
        }
        else
        {
            return { CliResultCode::failure
                   , fmt::format( "state save failed: {}"
                                , p.string() ) };
        }
    };
}

auto new_state( Kmap& kmap )
    -> std::function< CliCommandResult( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> CliCommandResult
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 
                        || args.size() == 1 );
            })
        ;

        // TODO: check for unsaved attributes... e.g., text buffers.
        kmap.reset();

        if( args.size() == 1 )
        {
            auto const p = FsPath{ args[ 0 ] };
            auto const succ = kmap.move_state( p );

            if( !succ )
            {
                return { CliResultCode::failure
                       , fmt::format( "move failed: {}"
                                    , p.string() ) };
            }
        }

        return { CliResultCode::success
               , "new kmap created" };
    };
}

namespace load_state_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;

const path = args.get( 0 );

if( kmap.fs_path_exists( path ) )
{
    rv = kmap.load_state( path );
}
else
{
    rv = kmap.failure( "file not found" );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "creates a taint project at nearest project category";
auto const arguments = std::vector< Argument >{ Argument{ "map_file_path"
                                                        , "path to map file"
                                                        , "filesystem_path" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    load.state
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace load_state_def

} // namespace kmap::cmd
