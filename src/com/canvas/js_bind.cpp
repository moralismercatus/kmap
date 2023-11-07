/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/canvas.hpp>
#include <com/canvas/common.hpp>
#include <kmap/binding/js/result.hpp>

#include <emscripten/bind.h>

namespace kmap::com::binding {

using namespace emscripten;

struct Canvas
{
    kmap::Kmap& km;

    Canvas( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto apply_layout( std::string const& json_contents ) const
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const canvas = KTRYE( km.fetch_component< kmap::com::Canvas >() );

        KTRY( canvas->apply_layout( json_contents ) );

        return outcome::success();
    }

    auto complete_path( std::string const& path ) const
        -> StringVec
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::Canvas >() )->complete_path( path );
    }

    auto fetch_base( Uuid const& pane ) const
        -> kmap::Result< float >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->pane_base( pane );
    }

    auto fetch_orientation( Uuid const& pane ) const
        -> kmap::Result< kmap::com::Orientation >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->pane_orientation( pane );
    }

    auto fetch_pane( std::string const& path ) const
        -> kmap::Result< Uuid >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->fetch_pane( path );
    }
    
    auto focus( Uuid const& pane )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->focus( pane );
    }

    auto hide( Uuid const& pane )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->hide( pane );
    }

    auto orient( Uuid const& pane
               , kmap::com::Orientation const orientation )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->orient( pane, orientation );
    }

    auto rebase( Uuid const& pane
               , float const base )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->rebase( pane, base );
    }

    auto redraw()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->redraw();
    }

    auto reorient( Uuid const& pane )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->reorient( pane );
    }

    auto reveal( Uuid const& pane )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->reveal( pane );
    }

    auto subdivide( Uuid const& pane
                  , std::string const& heading
                  , std::string const& orientation
                  , float const base
                  , std::string const& elem_type )
        -> kmap::Result< Uuid >
    {
        KM_RESULT_PROLOG();

        auto const parsed_orient = KTRY( from_string< Orientation >( orientation ) );

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->subdivide( pane, heading, Division{ parsed_orient, base, false, elem_type } );
    }

    auto toggle_pane( Uuid const& pane )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto canvas = KTRY( km.fetch_component< kmap::com::Canvas >() );
        auto const hidden = KTRY( canvas->pane_hidden( pane ) );

        if( hidden )
        {
            return canvas->reveal( pane );
        }
        else
        {
            return canvas->hide( pane );
        }
    }

    auto update_all_panes()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::Canvas >() )->update_all_panes();
    }

    auto breadcrumb_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->breadcrumb_pane(); } 
    auto breadcrumb_table_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->breadcrumb_table_pane(); } 
    auto canvas_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->canvas_pane(); } 
    auto cli_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->cli_pane(); } 
    auto completion_overlay() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->completion_overlay(); }
    auto editor_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->editor_pane(); } 
    auto jump_stack_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->jump_stack_pane(); }
    auto network_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->network_pane(); }
    auto preview_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->preview_pane(); }
    auto text_area_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->text_area_pane(); }
    auto workspace_pane() const -> Uuid { KM_RESULT_PROLOG(); return KTRYE( km.fetch_component< kmap::com::Canvas >() )->workspace_pane(); }
};

auto canvas()
    -> binding::Canvas
{
    return binding::Canvas{ kmap::Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_canvas )
{
    function( "canvas", &kmap::com::binding::canvas );
    class_< kmap::com::binding::Canvas >( "Canvas" )
        .function( "apply_layout", &kmap::com::binding::Canvas::apply_layout )
        .function( "breadcrumb_pane", &kmap::com::binding::Canvas::breadcrumb_pane )
        .function( "breadcrumb_table_pane", &kmap::com::binding::Canvas::breadcrumb_table_pane )
        .function( "canvas_pane", &kmap::com::binding::Canvas::canvas_pane )
        .function( "cli_pane", &kmap::com::binding::Canvas::cli_pane )
        .function( "complete_path", &kmap::com::binding::Canvas::complete_path )
        .function( "completion_overlay", &kmap::com::binding::Canvas::completion_overlay )
        .function( "editor_pane", &kmap::com::binding::Canvas::editor_pane )
        .function( "jump_stack_pane", &kmap::com::binding::Canvas::jump_stack_pane )
        .function( "fetch_base", &kmap::com::binding::Canvas::fetch_base )
        .function( "fetch_orientation", &kmap::com::binding::Canvas::fetch_orientation )
        .function( "fetch_pane", &kmap::com::binding::Canvas::fetch_pane )
        .function( "focus", &kmap::com::binding::Canvas::focus )
        .function( "hide", &kmap::com::binding::Canvas::hide )
        .function( "network_pane", &kmap::com::binding::Canvas::network_pane )
        .function( "orient", &kmap::com::binding::Canvas::orient )
        .function( "preview_pane", &kmap::com::binding::Canvas::preview_pane )
        .function( "rebase", &kmap::com::binding::Canvas::rebase )
        .function( "redraw", &kmap::com::binding::Canvas::redraw )
        .function( "reorient", &kmap::com::binding::Canvas::reorient )
        .function( "reveal", &kmap::com::binding::Canvas::reveal )
        .function( "text_area_pane", &kmap::com::binding::Canvas::text_area_pane )
        .function( "toggle_pane", &kmap::com::binding::Canvas::toggle_pane )
        .function( "update_all_panes", &kmap::com::binding::Canvas::update_all_panes )
        .function( "workspace_pane", &kmap::com::binding::Canvas::workspace_pane )
        ;
    enum_< kmap::com::Orientation >( "Orientation" )
        .value( "horizontal", kmap::com::Orientation::horizontal )
        .value( "vertical", kmap::com::Orientation::vertical )
        ;
}
    
KMAP_BIND_RESULT( kmap::com::Orientation );

} // namespace kmap::com::binding
