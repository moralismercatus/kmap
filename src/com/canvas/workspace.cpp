/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/workspace.hpp>

#include <com/canvas/pane_clerk.hpp>

namespace kmap::com {

Workspace::Workspace( Kmap& km
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ km, requisites, description }
    , pclerk_{ km }
{
    KM_RESULT_PROLOG();

    KTRYE( register_panes() );
}

auto Workspace::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( pclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto Workspace::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( pclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}
   
auto Workspace::register_panes()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    
    KTRY( pclerk_.register_pane( Pane{ .id = workspace_uuid
                                     , .heading = "workspace"
                                     , .division = Division{ Orientation::vertical, 0.025f, false, "div" } } ) );

    rv = outcome::success();

    return rv;
}

namespace {
namespace canvas_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Workspace
,   std::set({ "canvas"s })
,   "canvas related functionality"
);

} // namespace canvas_def 
}

} // namespace kmap::com
