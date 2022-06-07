/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "state.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../db.hpp"
#include "../error/master.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"

namespace kmap::cmd {

auto copy_state( Kmap& kmap )
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
        
        auto const& src = kmap.database()
                              .path();
        auto const& dst = FsPath{ args[ 0 ] };
        auto const succ = kmap.copy_state( dst );

        if( !succ )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "copy failed: {}", dst.string() ) );
        }

        return fmt::format( "{} copied to {}"
                          , src.string() 
                          , dst.string() );
    };
}

auto load_state( Kmap& kmap )
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

        auto const p = FsPath{ args[ 0 ] };
        auto const succ = kmap.load_state( p );

        if( !succ )
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "failed to load state: {}", p.string() ) );
        }
        else
        {
            return fmt::format( "loaded state: {}", p.string() );
        }
    };
}

auto save_state( Kmap& kmap )
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

        auto const p = FsPath{ args[ 0 ] };
        if( auto const succ = kmap.copy_state( p )
                           && kmap.load_state( p )
          ; succ )
        {
            return fmt::format( "state saved and loaded: {}", p.string() );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "state save failed: {}", p.string() ) );
        }
    };
}

auto new_state( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 
                        || args.size() == 1 );
            })
        ;

        // TODO: check for unsaved attributes... e.g., text buffers.
        KMAP_TRY( kmap.initialize() );

        if( args.size() == 1 )
        {
            auto const p = FsPath{ args[ 0 ] };
            auto const succ = kmap.move_state( p );

            if( !succ )
            {
                return KMAP_MAKE_ERROR_MSG( kmap::error_code::common::uncategorized, fmt::format( "move failed: {}" , p.string() ) );
            }
        }

        return "new kmap created";
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

auto const description = "loads state from disk";
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

namespace save_state_as_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;

const path = args.get( 0 );

if( !kmap.fs_path_exists( path ) )
{
    kmap.database().init_db_on_disk( path ).throw_on_error();
    kmap.database().flush_delta_to_disk().throw_on_error();

    rv = kmap.success( 'saved as: ' + path );
}
else
{
    rv = kmap.failure( "file already exists found" );
}

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "saves current state as new file on disk";
auto const arguments = std::vector< Argument >{ Argument{ "new_file_path"
                                                        , "path where state file will be saved. Appends \".kmap\" if no extension given."
                                                        , "filesystem_path" } };
auto const guard = Guard{ "unconditional"
                        , guard_code };
auto const action = action_code;

// TODO: ALIAS_COMMAND( save.as )
REGISTER_COMMAND
(
    save.state.as
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace save_state_to_disk_def

namespace save_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
if( kmap.database().has_file_on_disk() )
{
    return kmap.success( "file on disk found" );
}
else
{
    return kmap.failure( "failed to find file on disk" );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
let rv = null;

kmap.database().flush_delta_to_disk().throw_on_error();

rv = kmap.success( 'state saved to disk' );

return rv;
```)%%%";

using Guard = PreregisteredCommand::Guard;
using Argument = PreregisteredCommand::Argument;

auto const description = "saves current state to associated file on disk";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "file_is_on_disk"
                        , guard_code };
auto const action = action_code;

// TODO: ALIAS_COMMAND( save.state )?
REGISTER_COMMAND
(
    save
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace save_state_to_disk_def

} // namespace kmap::cmd
