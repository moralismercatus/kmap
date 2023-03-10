/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "emcc_bindings.hpp"

#include "com/database/db.hpp"
#include "com/network/network.hpp"
#include "com/tag/tag.hpp"
#include "com/text_area/text_area.hpp"
#include "error/cli.hpp"
#include "error/js_iface.hpp"
#include "error/master.hpp"
#include "error/network.hpp"
#include "filesystem.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "path/act/order.hpp"
#include "path/node_view.hpp"
#include "test/master.hpp"
#include "util/signal_exception.hpp"
#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <magic_enum.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/map.hpp>

#include <fstream>
#include <string>
#include <string_view>

using namespace emscripten;
using namespace kmap;
using namespace ranges;

namespace kmap::binding {

template< typename Enum >
auto register_enum( std::string_view const name )
    -> void
{
    auto reg_enum = emscripten::enum_< Enum >( name.data() );

    for( auto const& entries = magic_enum::enum_entries< Enum >()
       ; auto const& entry : entries )
    {
        reg_enum.value( entry.second.data(), entry.first );
    }
}

auto vector_string_from_int_ptr( uintptr_t vec )
    -> std::vector< std::string >*
{
    return reinterpret_cast< std::vector< std::string >* >( vec );
}

auto complete_child_heading( Uuid const& parent
                           , std::string const& input )
    -> StringVec
{
    auto& kmap = Singleton::instance();
    auto const completed_ms = complete_child_heading( kmap
                                                    , parent
                                                    , input );

    return completed_ms
         | views::values
         | to_vector;
}

auto complete_heading_path_from( Uuid const& root 
                               , Uuid const& selected
                               , std::string const& input )
    -> StringVec
{
    auto rv = StringVec{};
    auto& kmap = Singleton::instance();
    auto const completed_ms = complete_path_reducing( kmap
                                                    , root
                                                    , selected
                                                    , input );

    if( completed_ms )
    {
        rv = completed_ms.value()
           | views::transform( &CompletionNode::path )
           | to_vector;
    }

    return rv;
}

// TODO: Unify impl. with _from version.
auto complete_heading_path( std::string const& input )
    -> StringVec
{
    auto rv = StringVec{};
    auto& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );
    
    auto const completed_ms = complete_path_reducing( km
                                                    , km.root_node_id()
                                                    , nw->selected_node()
                                                    , input );

    if( completed_ms )
    {
        rv = completed_ms.value()
           | views::transform( &CompletionNode::path )
           | to_vector;
    }

    return rv;
}

auto count_ancestors( Uuid const& node )
    -> binding::Result< uint32_t >
{
    KM_RESULT_PROLOG();

    auto const& kmap = Singleton::instance();
    
    return kmap::Result< uint32_t >{ anchor::node( node ) | view2::ancestor | act2::count( kmap )  };
}

auto count_descendants( Uuid const& node )
    -> binding::Result< uint32_t >
{
    KM_RESULT_PROLOG();

    auto const& kmap = Singleton::instance();

    return kmap::Result< uint32_t >{ anchor::node( node ) | view2::desc | act2::count( kmap ) };
}

auto fetch_body( Uuid const& node )
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->fetch_body( node );
}

auto fetch_child( Uuid const& parent
                , std::string const& heading )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->fetch_child( parent, heading );
}

auto fetch_descendant( Uuid const& root
                     , std::string const& path )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return fetch_descendant( km
                           , root
                           , nw->selected_node()
                           , path );
}

// TODO: An alternative is to expose HeadingPath as a type to JS, and place e.g., HeadingPath::to_unique_node() a callable member. May overcomplicate the impl... It's more seamless to work in strings in JS.
auto fetch_node( std::string const& input )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return fetch_descendant( km
                           , km.root_node_id()
                           , nw->selected_node()
                           , input );
}

auto fetch_or_create_node( Uuid const& root
                         , std::string const& path )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return fetch_or_create_descendant( km
                                     , root
                                     , nw->adjust_selected( root )
                                     , path );
}

auto fetch_heading( Uuid const& node )
                
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->fetch_heading( node );
}

auto fetch_parent( Uuid const& child )
                
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->fetch_parent( child );
}

// TODO: Move to alias.cpp
auto create_alias( Uuid const& src 
                 , Uuid const& dst )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->create_alias( src, dst );
}

// TODO: Move to com//network.cpp
auto create_child( Uuid const& parent
                 , std::string const& title )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->create_child( parent
                           , format_heading( title )
                           , title );
}

auto create_descendant( Uuid const& parent
                      , std::string const& heading )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();

    return view::make( parent )
         | view::direct_desc( heading )
         | view::create_node( km )
         | view::to_single;
}

auto create_direct_descendant( Uuid const& parent
                             , std::string const& heading )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();

    return view::make( parent )
         | view::direct_desc( heading )
         | view::create_node( km )
         | view::to_single;
}

auto delete_children( Uuid const& parent )
    -> binding::Result< std::string > // TODO: Why is this returning std::string? Think it should be void.
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const res = anchor::node( parent )
                   | view2::child 
                   | act2::erase_node( km );
    
    if( !res )
    {
        return res.as_failure();
    }
    else
    {
        return kmap::Result< std::string >{ "children deleted" };
    }
}

auto delete_node( Uuid const& node )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->erase_node( node );
}

auto fetch_above( Uuid const& node )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto const& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->fetch_above( node );
}

auto fetch_below( Uuid const& node )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto const& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->fetch_below( node );
}

auto fetch_children( Uuid const& parent )
    -> UuidVec // embind natively supports std::vector, but not std::set, so prefer vector when feasible.
{
    auto const& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    return nw->fetch_children( parent ) | to< UuidVec >();
}

auto is_alias( Uuid const& node )
    -> bool 
{
    auto const& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    return nw->alias_store().is_alias( node );
}

auto is_alias_pair( Uuid const& src
                  , Uuid const& dst )
    -> bool 
{
    auto const& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    return nw->alias_store().is_alias( src, dst );
}

auto is_ancestor( Uuid const& ancestor
                , Uuid const& descendant )
    -> bool 
{
    auto const& km = Singleton::instance();

    return view::make( descendant )
         | view::ancestor( ancestor )
         | view::exists( km );
}

auto is_lineal( Uuid const& root 
              , Uuid const& node )
    -> bool 
{
    auto const& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    return nw->is_lineal( root, node );
}

auto is_signal_exception( intptr_t const exception_ptr )
    -> bool 
{
    auto* e = reinterpret_cast< std::exception* >( exception_ptr );

    return dynamic_cast< SignalException* >( e ) != nullptr;
}

auto handle_signal_exception( intptr_t const exception_ptr )
    -> void
{
    auto* e = reinterpret_cast< std::exception* >( exception_ptr );

    if( auto const sle = dynamic_cast< SignalLoadException* >( e )
      ; sle != nullptr )
    {
        auto& km = Singleton::instance();

        KTRYE( km.load( sle->db_path() ) );
    }
    else
    {
        KMAP_THROW_EXCEPTION_MSG( "should have converted to SignalException" );
    }
}

// TODO: This is incorrect. A heading is not identical to a heading path.
auto is_valid_heading( std::string const& heading )
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    if( kmap::is_valid_heading_path( heading ) )
    {
        return kmap::Result< std::string >{ "heading valid" };
    }
    else
    {
        // TODO: Presumably, is_valid_heading can return info as to _why_ it's invalid, and this can be propagated here.
        return kmap::Result< std::string >{ KMAP_MAKE_ERROR( error_code::network::invalid_heading ) };
    }
}

auto is_valid_heading_path( std::string const& path )
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    if( kmap::is_valid_heading_path( path ) )
    {
        return kmap::Result< std::string >{ "heading valid" };
    }
    else
    {
        // TODO: Presumably, is_valid_heading_path can return info as to _why_ it's invalid, and this can be propagated here.
        //       I'm too new to Boost.Outcome to say how this can be supplied logically to outcome::result, without renaming to "is_malformed...".
        return kmap::Result< std::string >{ KMAP_MAKE_ERROR( error_code::network::invalid_heading ) };
    }
}

auto load( std::string const& fs_path )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();

    // TODO: load should return a Result<>, this should be propagated.
    if( kmap.load( fs_path ) )
    {
        return kmap::Result< std::string >{ "map loaded" };
    }
    else
    {
        return kmap::Result< std::string >{ KMAP_MAKE_ERROR( error_code::common::uncategorized ) };
    }
}

auto map_headings( UuidVec const& nodes )
    -> StringVec // TODO: Should this be a Result<>? It's possible a mapping could fail....
{
    auto& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );
    auto rv = StringVec{};

    for( auto const& e : nodes )
    {
        rv.emplace_back( KTRYE( nw->fetch_heading( e ) ) );
    }

    return rv;
}

auto move_node( Uuid const& src
              , Uuid const& dst )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->move_node( src, dst );
}

auto move_body( Uuid const& src
              , Uuid const& dst )
    -> binding::Result< void >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->move_body( src, dst );
}

auto on_leaving_editor()
    -> binding::Result< void >
{
    auto& kmap = Singleton::instance();
    
    return kmap.on_leaving_editor();
}

auto present_time()
    -> std::string
{
    return std::to_string( kmap::present_time() );
}

auto std_exception_to_string( intptr_t const exception_ptr )
    -> std::string
{
    return fmt::format( "{}", std::string( reinterpret_cast< std::exception* >( exception_ptr )->what() ) );
}

auto print_std_exception( intptr_t const exception_ptr )
    -> void
{
    fmt::print( "std::exception from exception_ptr:\n" );
    fmt::print( "{}\n", std_exception_to_string( exception_ptr ) );
}

auto travel_down()
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    
    return nw->travel_down();
}

auto travel_left()
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    
    return nw->travel_left();
}

auto travel_right()
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    
    return nw->travel_right();
}

auto travel_top()
    -> void
{
    auto& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );
    
    nw->travel_top();
}

auto travel_up()
    -> binding::Result< Uuid >
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
    auto& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );
    
    nw->travel_bottom();
}

auto throw_load_signal( std::string const& db_path )
    -> void
{
    throw SignalLoadException{ db_path };
}

auto update_body( Uuid const& node
                , std::string const& content )
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    
    KTRY( nw->update_body( node, content ) );

    // TODO: Return result from 'update_*' in case error occurred.
    return kmap::Result< std::string >{ "updated" };
}

auto update_heading( Uuid const& node
                   , std::string const& content )
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    
    KTRY( nw->update_heading( node, content ) );

    // TODO: Return result from 'update_*' in case error occurred.
    return kmap::Result< std::string >{ "updated" };
}

auto update_title( Uuid const& node
                 , std::string const& content )
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    KTRY( nw->update_title( node, content ) );

    // TODO: Return result from 'update_*' in case error occurred.
    return kmap::Result< std::string >{ "updated" };
}

auto uuid_from_string( std::string const& id )
    -> binding::Result< Uuid >
{
    return kmap::uuid_from_string( id );
}

auto uuid_to_string( Uuid const& id )
    -> binding::Result< std::string >
{
    return kmap::Result< std::string >{ to_string( id ) }; // TODO: This should either wrap the exception and propagate to Result, or change to_string to return a Result.
}

auto view_body()
    -> void
{
    auto& kmap = Singleton::instance();
    auto const tv = KTRYE( kmap.fetch_component< com::TextArea >() );
    
    tv->focus_preview();
}

auto resolve_alias( Uuid const& node )
    -> Uuid // TODO: should return Result< Uuid >?
{
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    return nw->resolve( node );
}

auto root_node()
    -> Uuid
{
    auto& kmap = Singleton::instance();

    return kmap.root_node_id();
}

auto run_unit_tests( std::string const& arg )
    -> binding::Result< std::string >
{
    // Automatically append standard options.
    auto const cmd_args = StringVec{ "kmap"
                                    , "--show_progress=true"
                                    , "--report_level=detailed"
                                    , "--log_level=all"
                                    , io::format( "--run_test={}\n", arg ) };
    auto const res = kmap::run_unit_tests( cmd_args );

    if( res == 0 )
    {
        return kmap::Result< std::string >{ "all tests executed successfully" };
    }
    else
    {
        return kmap::Result< std::string >{ KMAP_MAKE_ERROR( error_code::common::uncategorized ) };
        // TODO: return as part of payload.
        // io::format( "failure code: {}", res ) );
    }
}

auto select_node( Uuid const& node )
    -> binding::Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );

    return nw->select_node( node );
}

auto selected_node()
    -> Uuid
{
    auto& km = Singleton::instance();
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    return nw->selected_node();
}

auto execute_sql( std::string const& stmt )
    -> void
{
#if KMAP_DEBUG
    auto& kmap = Singleton::instance();
    auto const db = KTRYE( kmap.fetch_component< com::Database >() );

    for( auto const& e : db->execute_raw( stmt ) )
    {
        fmt::print( "{},{}\n"
                  , e.first
                  , e.second );
    }
#else
    fmt::print( "execute_sql() only available in debug build, to avoid shooting yourself in the foot, but allow for debugging\n" );
#endif // KMAP_DEBUG
}

auto make_failure( std::string const& msg )
    -> binding::Result< std::string >
{
    auto res = Result< std::string >{ KMAP_MAKE_ERROR_MSG( error_code::cli::failure, msg ) };
    auto bound_result = binding::Result< std::string >{ res };

    return bound_result;
}

auto make_success( std::string const& msg )
    -> binding::Result< std::string >
{
    return kmap::Result< std::string >( msg );
}

auto make_eval_failure( std::string const& msg )
    -> binding::Result< void >
{
    auto res = Result< void >{ KMAP_MAKE_ERROR_MSG( error_code::js::js_exception, msg ) };
    auto bound_result = binding::Result< void >{ res };

    return bound_result;
}

auto make_eval_success()
    -> binding::Result< void >
{
    return binding::Result< void >{ kmap::Result< void >{ outcome::success() } };
}

auto sort_children( Uuid const& parent )
    -> binding::Result< std::string > // TODO: return Result< void >? And let caller create a string success?
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    auto children = view::make( parent )
                  | view::child
                  | view::to_node_set( km )
                  | act::order( km );

    children |= actions::sort( [ & ]( auto const& lhs
                                    , auto const& rhs )
    {
        return KTRYE( nw->fetch_heading( lhs ) ) < KTRYE( nw->fetch_heading( rhs ) );
    } );

    KTRY( nw->reorder_children( parent, children ) );

    return kmap::Result< std::string >{ "children sorted" };
}

auto swap_nodes( Uuid const& lhs
               , Uuid const& rhs )
    -> binding::Result< std::string >
{
    KM_RESULT_PROLOG();

    auto& km = Singleton::instance();
    auto const nw = KTRY( km.fetch_component< com::Network >() );
    
    if( auto const r = nw->swap_nodes( lhs , rhs )
      ; !r )
    {
        return r.as_failure();
    }
    else
    {
        return kmap::Result< std::string >{ "nodes swapped" };
    }
}

} // namespace kmap::binding

EMSCRIPTEN_BINDINGS( kmap_module )
{
    // function( "edit_body", &kmap::binding::edit_body );
    // function( "fetch_nodes", &kmap::binding::fetch_nodes ); // This fetches one or more nodes.
    function( "complete_child_heading", &kmap::binding::complete_child_heading );
    function( "complete_heading_path", &kmap::binding::complete_heading_path );
    function( "complete_heading_path_from", &kmap::binding::complete_heading_path_from );
    function( "count_ancestors", &kmap::binding::count_ancestors );
    function( "count_descendants", &kmap::binding::count_descendants );
    function( "create_alias", &kmap::binding::create_alias );
    function( "create_child", &kmap::binding::create_child );
    function( "create_descendant", &kmap::binding::create_descendant );
    function( "create_direct_descendant", &kmap::binding::create_direct_descendant );
    function( "delete_children", &kmap::binding::delete_children );
    function( "delete_node", &kmap::binding::delete_node );
    function( "eval_failure", &kmap::binding::make_eval_failure );
    function( "eval_success", &kmap::binding::make_eval_success );
    function( "execute_sql", &kmap::binding::execute_sql );
    function( "failure", &kmap::binding::make_failure );
    function( "fetch_above", &kmap::binding::fetch_above );
    function( "fetch_below", &kmap::binding::fetch_below );
    function( "fetch_body", &kmap::binding::fetch_body );
    function( "fetch_child", &kmap::binding::fetch_child );
    function( "fetch_children", &kmap::binding::fetch_children );
    function( "fetch_descendant", &kmap::binding::fetch_descendant );
    function( "fetch_heading", &kmap::binding::fetch_heading );
    function( "fetch_node", &kmap::binding::fetch_node ); // This fetches a single node and errors if ambiguous.
    function( "fetch_or_create_node", &kmap::binding::fetch_or_create_node );
    function( "fetch_parent", &kmap::binding::fetch_parent );
    function( "is_alias", &kmap::binding::is_alias );
    function( "is_alias_pair", &kmap::binding::is_alias_pair );
    function( "is_ancestor", &kmap::binding::is_ancestor );
    function( "is_lineal", &kmap::binding::is_lineal );
    function( "is_signal_exception", &kmap::binding::is_signal_exception );
    function( "handle_signal_exception", &kmap::binding::handle_signal_exception );
    function( "is_valid_heading", &kmap::binding::is_valid_heading );
    function( "is_valid_heading_path", &kmap::binding::is_valid_heading_path );
    function( "load", &kmap::binding::load );
    function( "map_headings", &kmap::binding::map_headings );
    function( "move_body", &kmap::binding::move_body );
    function( "move_node", &kmap::binding::move_node );
    function( "on_leaving_editor", &kmap::binding::on_leaving_editor );
    function( "present_time", &kmap::binding::present_time );
    function( "print_std_exception", &kmap::binding::print_std_exception );
    function( "resolve_alias", &kmap::binding::resolve_alias );
    function( "root_node", &kmap::binding::root_node );
    function( "run_unit_tests", &kmap::binding::run_unit_tests );
    function( "select_node", &kmap::binding::select_node );
    function( "std_exception_to_string", &kmap::binding::std_exception_to_string );
    function( "selected_node", &kmap::binding::selected_node );
    function( "sort_children", &kmap::binding::sort_children );
    function( "success", &kmap::binding::make_success );
    function( "swap_nodes", &kmap::binding::swap_nodes );
    function( "throw_load_signal", &kmap::binding::throw_load_signal );
    function( "travel_bottom", &kmap::binding::travel_bottom );
    function( "travel_down", &kmap::binding::travel_down );
    function( "travel_left", &kmap::binding::travel_left );
    function( "travel_right", &kmap::binding::travel_right );
    function( "travel_top", &kmap::binding::travel_top );
    function( "travel_up", &kmap::binding::travel_up );
    function( "update_body", &kmap::binding::update_body );
    function( "update_heading", &kmap::binding::update_heading );
    function( "update_title", &kmap::binding::update_title );
    function( "uuid_from_string", &kmap::binding::uuid_from_string );
    function( "uuid_to_string", &kmap::binding::uuid_to_string );

    function( "view_body", &kmap::binding::view_body );

    class_< Uuid >( "Uuid" )
        ;

    register_vector< std::string >( "VectorString" ).constructor( &kmap::binding::vector_string_from_int_ptr, allow_raw_pointers() );
    // Note: Don't leave spaces between <> in the names, as this introduces superflous "$" characters.
    register_vector< uint8_t >( "std::vector<uint8_t>" );
    register_vector< uint16_t >( "std::vector<uint16_t>" );
    register_vector< uint32_t >( "std::vector<uint32_t>" );
    register_vector< uint64_t >( "std::vector<uint64_t>" );
    register_vector< Uuid >( "std::vector<Uuid>" );
    class_< std::set< uint64_t > >( "std::set<uint64_t>" )
        ;
    class_< std::set< Uuid > >( "std::set<Uuid>" )
        ;

    KMAP_BIND_RESULT( Uuid );
    KMAP_BIND_RESULT( double );
    KMAP_BIND_RESULT( float );
    KMAP_BIND_RESULT( std::set<Uuid> );
    KMAP_BIND_RESULT( std::set<uint16_t> );
    KMAP_BIND_RESULT( std::set<uint32_t> );
    KMAP_BIND_RESULT( std::set<uint64_t> );
    KMAP_BIND_RESULT( std::set<uint8_t> );
    KMAP_BIND_RESULT( std::string );
    KMAP_BIND_RESULT( std::vector<Uuid> );
    KMAP_BIND_RESULT( std::vector<uint16_t> );
    KMAP_BIND_RESULT( std::vector<uint32_t> );
    KMAP_BIND_RESULT( std::vector<uint64_t> );
    KMAP_BIND_RESULT( std::vector<uint8_t> );
    KMAP_BIND_RESULT( uint16_t );
    KMAP_BIND_RESULT( uint32_t );
    KMAP_BIND_RESULT( uint64_t );
    KMAP_BIND_RESULT( uint8_t );
    class_< kmap::binding::Result< void > >( "Result<void>" )
        .function( "error_message", &kmap::binding::Result< void >::error_message )
        .function( "has_error", &kmap::binding::Result< void >::has_error )
        .function( "has_value", &kmap::binding::Result< void >::has_value )
        .function( "throw_on_error", &kmap::binding::Result< void >::throw_on_error )
        .function( "throw_on_error", &kmap::binding::Result< void >::throw_on_error )
        ;
}
