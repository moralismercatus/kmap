/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/layout.hpp>

#include <com/filesystem/filesystem.hpp>
#include <com/option/option_clerk.hpp>

namespace kmap::com {

Layout::Layout( Kmap& km
                    , std::set< std::string > const& requisites
                    , std::string const& description )
    : Component{ km, requisites, description }
    , oclerk_{ km }
{
    KM_RESULT_PROLOG();

    KTRYE( register_options() );
}

auto Layout::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( oclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto Layout::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( oclerk_.check_registered() );

    rv = outcome::success();

    return rv;
}
   
auto Layout::register_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    
    // initial_layout
    {
        auto const value = KTRY( load_initial_layout( kmap_root_dir / "initial_layout.json" ) );
        auto const action = 
R"%%%(
const canvas = kmap.canvas();
ktry( canvas.apply_layout( JSON.stringify( option_value ) ) );
canvas.update_all_panes();
)%%%";
        KTRY( oclerk_.register_option( Option{ .heading = "canvas.initial_layout"
                                             , .descr = "Applies layout specified in value. In particular, for the initial pane layout."
                                             , .value = value 
                                             , .action = action } ) );
    }

    rv = outcome::success();

    return rv;
}

namespace {
namespace canvas_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Layout
,   std::set({ "canvas"s, "option_store"s })
,   "makes initial layout for panes available"
);

} // namespace canvas_def 
}

} // namespace kmap::com
