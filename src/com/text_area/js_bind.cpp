/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/text_area/text_area.hpp>

#include <emscripten/val.h>

namespace kmap::com {
namespace binding {

using namespace emscripten;

struct TextArea
{
    Kmap& km;

    TextArea( Kmap& kmap )
        : km{ kmap }
    {
    }

    auto focus_editor()
        -> void
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

       tv->focus_editor();
    }

    auto load_preview( Uuid const& node )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRY( km.fetch_component< com::TextArea >() );

        return tv->load_preview( node );
    }

    auto rebase_pane( float percent )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->rebase_pane( percent );
    }

    auto rebase_preview_pane( float percent )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->rebase_preview_pane( percent );
    }
    
    auto set_editor_text( std::string const& text )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRYE( km.fetch_component< com::TextArea >() );

        tv->set_editor_text( text );
    }

    auto show_editor()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRY( km.fetch_component< com::TextArea >() );

        return tv->show_editor();
    }

    auto show_preview( std::string const& body_text )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const tv = KTRY( km.fetch_component< com::TextArea >() );

        return tv->show_preview( body_text );
    }
};

auto text_area()
    -> binding::TextArea
{
    auto& kmap = Singleton::instance();

    return binding::TextArea{ kmap };
}

EMSCRIPTEN_BINDINGS( kmap_text_area )
{
    function( "text_area", &kmap::com::binding::text_area );
    class_< kmap::com::binding::TextArea >( "TextArea" )
        .function( "focus_editor", &kmap::com::binding::TextArea::focus_editor )
        .function( "load_preview", &kmap::com::binding::TextArea::load_preview )
        .function( "rebase_pane", &kmap::com::binding::TextArea::rebase_pane )
        .function( "rebase_preview_pane", &kmap::com::binding::TextArea::rebase_preview_pane )
        .function( "set_editor_text", &kmap::com::binding::TextArea::set_editor_text )
        .function( "show_editor", &kmap::com::binding::TextArea::show_editor )
        .function( "show_preview", &kmap::com::binding::TextArea::show_preview )
        ;
}

} // namespace binding
} // namespace kmap::com
