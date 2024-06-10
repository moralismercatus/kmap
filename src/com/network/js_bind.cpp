/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/network/network.hpp>
#include <js/iface.hpp>
#include <kmap.hpp>
#include <path.hpp>
#include <util/result.hpp>

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

    auto create_alias( Uuid const& src 
                     , Uuid const& dst )
        -> Result< Uuid >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH( "src", src );
            KM_RESULT_PUSH( "dst", dst );
            
        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->create_alias( src, dst );
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
    auto fetch_body( Uuid const& node )
        -> Result< std::string >
    {
        KM_RESULT_PROLOG();

        auto const nw = KTRY( kmap_.fetch_component< com::Network >() );

        return nw->fetch_body( node );
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
    auto reorder_children( Uuid const& parent
                         , std::vector< Uuid > const& children )
        -> void
    {
        KM_RESULT_PROLOG();

        return KTRYE( KTRYE( kmap_.fetch_component< com::Network >() )->reorder_children( parent, children ) );
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
    auto sort_children_prompt( Uuid const& node ) // Not exactly sure where this belongs.. It's too specific to belong in network. It's a kind of utility. Also means doesn't belong in NetworkCommand, right?
        -> Result< void >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_NODE( "node", node );

        // auto const nw = KTRY( kmap_.fetch_component< com::Network >() );
        // auto cso = KTRY( nw->fetch_children_ordered( node ) );
        // auto const comp_fn = [ & ]( auto const& lhsn, auto const& rhsn ) -> bool
        // {
        //     auto const jscript = fmt::format( "kmap_sort_children_prompt( {} );"
        //                                     , to_string( lhsn ) );
        //     KTRYE( js::eval_void( jscript ) );
        //     KTRYE( js::call( "kmap_sort_children_prompt", node ) );

        //     auto const res_str = KTRYE( js::eval< std::string >( "return kmap_detail_async_sort_comparator_ret_val;" ) );
        //     auto const res_id = KTRYE( uuid_from_string( res_str ) );

        //     return ( res_id == lhsn );
    // return KTRYE( kmap::js::eval< bool >( fmt::format( "return confirm( \"left or right?\" );" ) ) );

        // };

        // fmt::print( "inNetwork::sort_shildren_prompt\n" );

        // std::sort( cso.begin(), cso.end(), comp_fn );

        // KTRY( nw->reorder_children( node, cso ) );

        KTRY( js::call_rvoid( "kmap_sort_children_prompt", node ) );

        return outcome::success();
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

    auto update_body( Uuid const& node
                    , std::string const& content )
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRY( km.fetch_component< com::Network >() );
        
        KTRY( nw->update_body( node, content ) );

        return outcome::success();
    }

    auto update_heading( Uuid const& node
                       , std::string const& heading )
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRY( km.fetch_component< com::Network >() );
        
        KTRY( nw->update_heading( node, heading ) );

        return outcome::success();
    }

    auto update_title( Uuid const& node
                     , std::string const& title )
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto& km = Singleton::instance();
        auto const nw = KTRY( km.fetch_component< com::Network >() );
        
        KTRY( nw->update_title( node, title ) );

        return outcome::success();
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
        .function( "create_alias", &kmap::com::binding::Network::create_alias )
        .function( "create_child", &kmap::com::binding::Network::create_child )
        .function( "erase_node", &kmap::com::binding::Network::erase_node )
        .function( "fetch_body", &kmap::com::binding::Network::fetch_body )
        .function( "fetch_node", &kmap::com::binding::Network::fetch_node )
        .function( "fetch_parent", &kmap::com::binding::Network::fetch_parent )
        .function( "fetch_title", &kmap::com::binding::Network::fetch_title )
        // .function( "move_children", &kmap::com::binding::Network::move_children )
        .function( "is_alias", &kmap::com::binding::Network::is_alias )
        .function( "move_node", &kmap::com::binding::Network::move_node )
        .function( "reorder_children", &kmap::com::binding::Network::reorder_children )
        .function( "select_node", &kmap::com::binding::Network::select_node )
        .function( "selected_node", &kmap::com::binding::Network::selected_node )
        .function( "sort_children_prompt", &kmap::com::binding::Network::sort_children_prompt )
        .function( "travel_bottom", &kmap::com::binding::Network::travel_bottom )
        .function( "travel_down", &kmap::com::binding::Network::travel_down )
        .function( "travel_left", &kmap::com::binding::Network::travel_left )
        .function( "travel_right", &kmap::com::binding::Network::travel_right )
        .function( "travel_top", &kmap::com::binding::Network::travel_top )
        .function( "travel_up", &kmap::com::binding::Network::travel_up )
        .function( "update_body", &kmap::com::binding::Network::update_body )
        .function( "update_heading", &kmap::com::binding::Network::update_heading )
        .function( "update_title", &kmap::com::binding::Network::update_title )
        ;
}

} // namespace binding
} // namespace kmap::com
