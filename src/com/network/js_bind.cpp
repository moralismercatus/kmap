/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/network/network.hpp>
#include <util/result.hpp>
#include <path.hpp>
#include <kmap.hpp>

#include <emscripten/bind.h>

namespace kmap::com {
namespace binding {

using namespace emscripten;

struct Network
{
    Kmap& kmap_;

    Network( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto create_child( Uuid const& parent
                     , std::string const& title )
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH( "parent", parent );
            KM_RESULT_PUSH( "title", title );
            
        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->create_child( parent, format_heading( title ), title );
    }
    auto erase_node( Uuid const& node )
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH( "node", node );

        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->erase_node( node );
    }
    auto fetch_node( std::string const& path )
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_STR( "path", path );

        auto rv = KMAP_MAKE_RESULT( Uuid );
        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );
        auto const rs = KTRY( decide_path( kmap_, nw->root_node(), nw->selected_node(), path ) );
        
        if( rs.size() == 1 )
        {
            rv = rs.back();
        }

        return rv;
    }
    auto fetch_parent( Uuid const& node )
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_NODE( "node", node );

        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->fetch_parent( node );
    }
    auto fetch_title( Uuid const& node )
        -> Result< std::string >
    {
        KM_RESULT_PROLOG();

        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->fetch_title( node );
    }
    auto is_alias( Uuid const& node )
        -> bool 
    {
        KM_RESULT_PROLOG();

        auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );

        return nw->is_alias( node );
    }
    auto move_node( Uuid const& src
                  , Uuid const& dst )
        -> Result< void >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH( "src", src );
            KM_RESULT_PUSH( "dst", dst );

        auto rv = KMAP_MAKE_RESULT( void );

        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        KTRY( nw->move_node( src, dst ) );

        rv = outcome::success();

        return rv;
    }
    auto selected_node()
        -> Uuid
    {
        KM_RESULT_PROLOG();

        return KTRYE( kmap_.fetch_component< com::Network >() )->selected_node();
    }
    auto select_node( Uuid const& node )
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();
            // KM_RESULT_PUSH_NODE( "node", node );

        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->select_node( node );
    }
    auto travel_down()
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRY( km.fetch_component< com::Network >() );
        
        return nw->travel_down();
    }
    auto travel_left()
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRY( km.fetch_component< com::Network >() );
        
        return nw->travel_left();
    }
    auto travel_right()
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRY( km.fetch_component< com::Network >() );
        
        return nw->travel_right();
    }
    auto travel_top()
        -> void
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRYE( km.fetch_component< com::Network >() );
        
        nw->travel_top();
    }
    auto travel_up()
        -> Result< Uuid >
    {
        KMAP_PROFILE_SCOPE();
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRY( km.fetch_component< com::Network >() );
        
        return nw->travel_up();
    }
    auto travel_bottom()
        -> void
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRYE( km.fetch_component< com::Network >() );
        
        nw->travel_bottom();
    }
};

auto network()
    -> binding::Network
{
    return binding::Network{ Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_network )
{
    function( "network", &kmap::com::binding::network );
    class_< kmap::com::binding::Network >( "Network" )
        .function( "create_child", &kmap::com::binding::Network::create_child )
        .function( "erase_node", &kmap::com::binding::Network::erase_node )
        .function( "fetch_node", &kmap::com::binding::Network::fetch_node )
        .function( "fetch_parent", &kmap::com::binding::Network::fetch_parent )
        .function( "fetch_title", &kmap::com::binding::Network::fetch_title )
        // .function( "move_children", &kmap::com::binding::Network::move_children )
        .function( "is_alias", &kmap::com::binding::Network::is_alias )
        .function( "move_node", &kmap::com::binding::Network::move_node )
        .function( "select_node", &kmap::com::binding::Network::select_node )
        .function( "selected_node", &kmap::com::binding::Network::selected_node )
        .function( "travel_bottom", &kmap::com::binding::Network::travel_bottom )
        .function( "travel_down", &kmap::com::binding::Network::travel_down )
        .function( "travel_left", &kmap::com::binding::Network::travel_left )
        .function( "travel_right", &kmap::com::binding::Network::travel_right )
        .function( "travel_top", &kmap::com::binding::Network::travel_top )
        .function( "travel_up", &kmap::com::binding::Network::travel_up )
        ;
}

} // namespace binding
} // namespace kmap::com
