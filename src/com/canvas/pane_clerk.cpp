/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/pane_clerk.hpp>

#include <com/canvas/canvas.hpp>
#include <error/master.hpp>
#include <kmap.hpp>
#include <path/node_view2.hpp>
#include <util/clerk/clerk.hpp>
#include <util/result.hpp>
#include <utility.hpp>

#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>

namespace rvs = ranges::views;

namespace kmap::com {

PaneClerk::PaneClerk( Kmap& km )
    : kmap_{ km }
{
}

PaneClerk::~PaneClerk()
{
    try
    {
        if( auto const canvasc = kmap_.fetch_component< com::Canvas >()
          ; canvasc )
        {
            auto& canvas = canvasc.value();
            for( auto const& e : installed_panes_
                               | rvs::values
                               | rvs::reverse )
            {
                // TODO:
                // if( auto const res = canvas->erase_subdivision( e )
                // ; !res && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
                // {
                //     KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
                // }
            }
            for( auto const& e : installed_overlays_
                               | rvs::values
                               | rvs::reverse )
            {
                // TODO:
                // if( auto const res = canvas->erase_subdivision( e )
                // ; !res && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
                // {
                //     KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
                // }
            }
        }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto PaneClerk::check_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& pane : registered_panes_
                          | ranges::views::values )
    {
          
        if( !( view2::canvas::canvas_root
             | view2::desc( pane.id )
             | act2::exists( kmap_ ) ) )
        {
            KTRY( install_pane( pane ) );
        }
        if( !js::element_exists( to_string( pane.id ) ) )
        {
            auto const canvas = KTRY( kmap_.fetch_component< com::Canvas >() );

            KTRY( canvas->create_html_element( pane.id ) );
        }
        // TODO: For consistency, shouldn't we check for html element for pane.id as well?
    }
    for( auto const& overlay : registered_overlays_
                             | ranges::views::values )
    {
        auto const vnode = anchor::node( overlay.id );
        auto const matches = [ & ]() -> bool
        {
            return vnode | act2::exists( kmap_ )
                && util::match_raw_body( kmap_, vnode | view2::child( "type" ), overlay.elem_type )
                ;
        }();

        if( !matches )
        {
            auto const reinstall = KTRY( util::confirm_reinstall( "overlay", overlay.heading ) );

            if( reinstall )
            {
                if( vnode | act2::exists( kmap_ ) )
                {
                    KTRY( vnode | act2::erase_node( kmap_ ) );
                }

                KTRY( install_overlay( overlay ) ); // Re-install.
            }
        }

        if( !js::element_exists( to_string( overlay.id ) ) )
        {
            auto const canvas = KTRY( kmap_.fetch_component< com::Canvas >() );

            KTRY( canvas->create_html_element( overlay.id ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto PaneClerk::register_pane( Pane const& pane )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    
    KMAP_ENSURE( !registered_panes_.contains( pane.heading ), error_code::common::uncategorized );

    registered_panes_.insert( { pane.heading, pane } );

    rv = outcome::success();

    return rv;
}

auto PaneClerk::register_overlay( Overlay const& overlay )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    
    KMAP_ENSURE( !registered_overlays_.contains( overlay.heading ), error_code::common::uncategorized );

    registered_overlays_.insert( { overlay.heading, overlay } );

    rv = outcome::success();

    return rv;
}

auto PaneClerk::install_pane( Pane const& pane )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", pane.heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const canvas = KTRY( kmap_.fetch_component< com::Canvas >() );
    auto const panen = KTRY( canvas->install_pane( pane ) );

    installed_panes_.insert( { pane.heading, panen } );

    rv = panen;

    return rv;
}

auto PaneClerk::install_overlay( Overlay const& overlay )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "heading", overlay.heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const canvas = KTRY( kmap_.fetch_component< com::Canvas >() );
    auto const overn = KTRY( canvas->install_overlay( overlay ) );

    installed_overlays_.insert( { overlay.heading, overn } );

    rv = overn;

    return rv;
}

auto PaneClerk::install_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& pane : registered_panes_
                          | ranges::views::values )
    {
        KTRY( install_pane( pane ) );
    }
    for( auto const& overlay : registered_overlays_
                             | ranges::views::values )
    {
        KTRY( install_overlay( overlay ) );
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap::com
