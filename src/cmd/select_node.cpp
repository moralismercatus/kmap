/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "select_node.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"
#include "command.hpp"
#include "com/cmd/command.hpp"
#include "com/network/network.hpp"

#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

using namespace ranges;

namespace kmap::cmd {

auto select_destination( Kmap& kmap )
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

        auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
        auto const heading = args[ 0 ];
        auto const selected = nw->selected_node();
        auto const dsts = nw->alias_store().fetch_aliases_to( selected );
        auto const map = dsts
                       | views::transform( [ & ]( auto const& e ){ return std::pair{ e, KTRYE( nw->fetch_heading( e ) ) }; } )
                       | to_vector;
        if( auto const it = find( map
                                , heading
                                , &std::pair< Uuid, Heading >::second )
          ; it != map.end() )
        {
            KTRYE( nw->select_node( it->first ) );

            return "alias destination selected";
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "node has no aliases" ) );
        }

    };
}

auto select_node( Kmap& kmap
                , std::string const& dst )
    -> Result< std::string >
{
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    if( auto const target = view::make( kmap.root_node_id() )
                          | view::desc( dst )
                          | view::fetch_node( kmap )
      ; target )
    {
        KTRY( nw->select_node( target.value() ) );

        return "selected";
    }
    else
    {
        return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "Target node not found: {}", dst ) );
    }
}

auto select_node( Kmap& kmap )
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

        return select_node( kmap
                          , args[ 0 ] );
    };
}

auto select_root( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    return [ & ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 );
                BC_ASSERT( nw->exists( "/" ) );
            })
        ;

        KTRYE( nw->select_node( kmap.root_node_id() ) );

        return "selected";
    };
}

auto select_source( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >
{
    return [ &kmap ]( com::CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 );
            })
        ;

        auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
        auto const selected = nw->selected_node();

        if( nw->alias_store().is_alias( selected ) )
        {
            KTRYE( nw->select_node( nw->alias_store().resolve( selected ) ) );

            return "alias source selected";
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "node is not an alias" ) );
        }

    };
}

#if 0
namespace travel_up_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.travel_up();

return kmap.success( 'traveled' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects node directly above selected, if it exists";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    travel.up
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace travel_up_def

namespace travel_down_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.travel_down();

return kmap.success( 'traveled' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects node directly above selected, if it exists";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    travel.down
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace travel_down_def

namespace travel_left_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.travel_left();

return kmap.success( 'traveled' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects node directly above selected, if it exists";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    travel.left
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace travel_left_def

namespace travel_right_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.travel_right();

return kmap.success( 'traveled' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects node directly above selected, if it exists";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    travel.right
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace travel_right_def

namespace travel_top_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.travel_top();

return kmap.success( 'traveled' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects node directly above selected, if it exists";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    travel.top
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace travel_top_def

namespace travel_bottom_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.travel_bottom();

return kmap.success( 'traveled' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects node directly above selected, if it exists";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    travel.bottom
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace travel_bottom_def

namespace resolve_alias_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
if( kmap.is_alias( kmap.selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'non-alias' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
const selected = kmap.selected_node();

kmap.select_node( kmap.resolve_alias( selected ) );

return kmap.success( 'alias resolved' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects underlying source node of selected alias";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_alias", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    resolve.alias
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace resolve_alias_def

namespace resolve_def {
namespace { 

auto const guard_code =
R"%%%(```javascript
if( kmap.is_alias( kmap.selected_node() ) )
{
    return kmap.success( 'success' );
}
else
{
    return kmap.failure( 'non-alias' );
}
```)%%%";
auto const action_code =
R"%%%(```javascript
const selected = kmap.selected_node();

kmap.select_node( kmap.resolve_alias( selected ) );

return kmap.success( 'alias resolved' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "selects underlying source node of selected alias";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "is_alias", guard_code };
auto const action = action_code;

REGISTER_COMMAND // TODO: Use REGISTER_COMMAND_ALIAS instead.
(
    resolve
,   description 
,   arguments
,   guard
,   action
);

} // namespace anon
} // namespace resolve_def
#endif // 0


} // namespace kmap::cmd