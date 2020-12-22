/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "emcc_bindings.hpp"

#include "error/cli.hpp"
#include "error/master.hpp"
#include "error/network.hpp"
#include "filesystem.hpp"
#include "io.hpp"
#include "jump_stack.hpp"
#include "kmap.hpp"
#include "test/master.hpp"
#include "utility.hpp"

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/map.hpp>

#include <fstream>
#include <string>

using namespace emscripten;
using namespace kmap;
using namespace ranges;

namespace kmap::binding {

// Note: These class wrappers are needed for global types, as Embind doesn't take well to references nor pointers,
//       so the whole class ends up being copied; thus, I am providing simple wrappers to get copied.
struct Cli
{
    auto notify_success( std::string const& message )
        -> void
    {
        auto& kmap = Singleton::instance();

        kmap.cli().notify_success( message );
    }
    auto notify_failure( std::string const& message )
        -> void
    {
        auto& kmap = Singleton::instance();

        kmap.cli().notify_failure( message );
    }
    auto reset_all_preregistered()
        -> bool
    {
        auto& kmap = Singleton::instance();

        return kmap.cli().reset_all_preregistered();
    }
};

struct JumpStack
{
    auto jump_in( Uuid const& node )
        -> void
    {
        auto& kmap = Singleton::instance();

        kmap.jump_stack().jump_in( node );
    }
};

auto vector_string_from_int_ptr( uintptr_t vec )
    -> std::vector< std::string >*
{
    return reinterpret_cast< std::vector< std::string >* >( vec );
}

auto cli_on_key_up( int const key
                  , bool const is_ctrl
                  , bool const is_shift  )
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.cli_on_key_up( key
                      , is_ctrl 
                      , is_shift );
}

auto cli()
    -> binding::Cli
{
    return binding::Cli{};
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

auto complete_cli( std::string const& input )
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.complete_cli( input );
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
    auto& kmap = Singleton::instance();
    
    auto const completed_ms = complete_path_reducing( kmap
                                                    , kmap.root_node_id()
                                                    , kmap.selected_node()
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
    auto const& kmap = Singleton::instance();
    
    return kmap::Result< uint32_t >{ kmap.count_ancestors( node ) };
}

auto count_descendants( Uuid const& node )
    -> binding::Result< uint32_t >
{
    auto const& kmap = Singleton::instance();

    return kmap::Result< uint32_t >{ kmap.count_descendants( node ) };
}

auto fetch_body( Uuid const& node )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();

    return kmap.fetch_body( node );
}

auto fetch_child( Uuid const& parent
                , std::string const& heading )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.fetch_child( parent, heading );
}

auto fetch_descendant( Uuid const& root
                            , std::string const& path )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.fetch_descendant( root, path );
}

// TODO: An alternative is to expose HeadingPath as a type to JS, and place e.g., HeadingPath::to_unique_node() a callable member. May overcomplicate the impl... It's more seamless to work in strings in JS.
auto fetch_node( std::string const& input )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.fetch_descendant( input );
}

auto fetch_or_create_node( Uuid const& root
                         , std::string const& path )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.fetch_or_create_descendant( root, path );
}

auto fetch_heading( Uuid const& node )
                
    -> binding::Result< std::string >
{
    auto const& kmap = Singleton::instance();

    return kmap.fetch_heading( node );
}

auto fetch_parent( Uuid const& child )
                
    -> binding::Result< Uuid >
{
    auto const& kmap = Singleton::instance();

    return kmap.fetch_parent( child );
}

auto create_alias( Uuid const& src 
                 , Uuid const& dst )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return  kmap.create_alias( src, dst );
}

auto create_child( Uuid const& parent
                 , std::string const& title )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.create_child( parent
                            , format_heading( title )
                            , title );
}

auto delete_alias( Uuid const& node )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.delete_alias( node );
}

auto delete_children( Uuid const& parent )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();
    auto const res = kmap.delete_children( parent );
    
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
    auto& kmap = Singleton::instance();

    return kmap.delete_node( node );
}

auto edit_body() // TODO: Should this take a target node?
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.load_body_editor( kmap.selected_node() );
}

auto focus_network()
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.focus_network();
}

auto fs_path_exists( std::string const& path )
    -> bool 
{
    namespace fs = boost::filesystem;

    return fs::exists( kmap_root_dir / path );
}

auto fetch_above( Uuid const& node )
    -> binding::Result< Uuid >
{
    auto const& kmap = Singleton::instance();

    return kmap.fetch_above( node );
}

auto fetch_below( Uuid const& node )
    -> binding::Result< Uuid >
{
    auto const& kmap = Singleton::instance();

    return kmap.fetch_below( node );
}

auto fetch_children( Uuid const& parent )
    -> UuidVec // embind natively supports std::vector, but not std::set, so prefer vector when feasible.
{
    auto const& kmap = Singleton::instance();

    return kmap.fetch_children( parent ) | to< UuidVec >();
}

auto is_alias( Uuid const& node )
    -> bool 
{
    auto const& kmap = Singleton::instance();

    return kmap.is_alias( node );
}

auto is_alias_pair( Uuid const& src
                  , Uuid const& dst )
    -> bool 
{
    auto const& kmap = Singleton::instance();

    return kmap.is_alias( src, dst );
}

auto is_ancestor( Uuid const& ancestor
                , Uuid const& descendant )
    -> bool 
{
    auto const& kmap = Singleton::instance();

    return kmap.is_ancestor( ancestor
                           , descendant );
}

auto is_in_tree( Uuid const& root 
               , Uuid const& node )
    -> bool 
{
    auto const& kmap = Singleton::instance();

    return kmap.is_in_tree( root
                          , node );
}

auto is_lineal( Uuid const& root 
              , Uuid const& node )
    -> bool 
{
    auto const& kmap = Singleton::instance();

    return kmap.is_lineal( root
                         , node );
}

auto is_valid_heading( std::string const& heading )
    -> binding::Result< std::string >
{
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

auto jump_in()
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.jump_in();
}

auto jump_out()
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.jump_out();
}

auto jump_stack()
    -> binding::JumpStack
{
    return binding::JumpStack{};
}

auto load_state( std::string const& fs_path )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();

    // TODO: load_state should return a Result<>, this should be propagated.
    if( kmap.load_state( fs_path ) )
    {
        return kmap::Result< std::string >{ "map loaded" };
    }
    else
    {
        return kmap::Result< std::string >{ KMAP_MAKE_ERROR( error_code::common::unknown ) };
    }
}

auto map_headings( UuidVec const& nodes )
    -> StringVec // TODO: Should this be a Result<>? It's possible a mapping could fail....
{
    auto& kmap = Singleton::instance();
    auto rv = StringVec{};

    for( auto const& e : nodes )
    {
        rv.emplace_back( kmap.fetch_heading( e ).value() );
    }

    return rv;
}

auto move_node( Uuid const& src
              , Uuid const& dst )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.move_node( src, dst );
}

auto move_body( Uuid const& src
              , Uuid const& dst )
    -> bool
{
    auto& kmap = Singleton::instance();

    return kmap.move_body( src, dst );
}

auto on_leaving_editor()
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.on_leaving_editor();
}

auto parse_cli( std::string const& input )
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.parse_cli( input );
}

auto travel_down()
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();
    
    return kmap.travel_down();
}

auto travel_left()
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();
    
    return kmap.travel_left();
}

auto travel_right()
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();
    
    return kmap.travel_right();
}

auto travel_top()
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.travel_top();
}

auto travel_up()
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();
    
    return kmap.travel_up();
}

auto travel_bottom()
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.travel_bottom();
}

auto update_body( Uuid const& node
                , std::string const& content )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();
    
    kmap.update_body( node, content );

    // TODO: Return result from 'update_*' in case error occurred.
    return kmap::Result< std::string >{ "updated" };
}

auto update_heading( Uuid const& node
                   , std::string const& content )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();
    
    kmap.update_heading( node, content );

    // TODO: Return result from 'update_*' in case error occurred.
    return kmap::Result< std::string >{ "updated" };
}

auto update_title( Uuid const& node
                 , std::string const& content )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();

    kmap.update_title( node, content );

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
    auto& tv = kmap.text_view();
    
    tv.focus_preview();
}

auto resolve_alias( Uuid const& node )
    -> Uuid // TODO: should return Result< Uuid >?
{
    auto& kmap = Singleton::instance();

    return kmap.resolve( node );
}

auto root_node()
    -> Uuid
{
    auto& kmap = Singleton::instance();

    return kmap.root_node_id();
}

auto run_unit_tests()
    -> binding::Result< std::string >
{
    auto const res = kmap::run_unit_tests();

    if( res == 0 )
    {
        return kmap::Result< std::string >{ "all tests executed successfully" };
    }
    else
    {
        return kmap::Result< std::string >{ KMAP_MAKE_ERROR( error_code::common::unknown ) };
        // TODO: return as part of payload.
        // io::format( "failure code: {}", res ) );
    }
}

auto select_node( Uuid const& node )
    -> bool
{
    auto& kmap = Singleton::instance();

    return kmap.select_node( node );
}

auto selected_node()
    -> Uuid
{
    auto& kmap = Singleton::instance();

    return kmap.selected_node();
}

auto execute_sql( std::string const& stmt )
    -> void
{
#if KMAP_DEBUG
    auto& kmap = Singleton::instance();
    auto& db = kmap.database();

    for( auto const& e : db.execute_raw( stmt ) )
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
    auto res = Result< std::string >{ KMAP_MAKE_ERROR( error_code::cli::failure ) };
    auto bound_result = binding::Result< std::string >{ res };

    bound_result.failure_message = msg;

    return bound_result;
}

auto make_success( std::string const& msg )
    -> binding::Result< std::string >
{
    return kmap::Result< std::string >( msg );
}

auto sort_children( Uuid const& parent )
    -> binding::Result< std::string > // TODO: return Result< void >? And let caller create a string success?
{
    auto& kmap = Singleton::instance();
    auto children = kmap.fetch_children_ordered( parent );

    children |= actions::sort( [ & ]( auto const& lhs
                                    , auto const& rhs )
    {
        return kmap.fetch_heading( lhs ).value() < kmap.fetch_heading( rhs ).value();
    } );

    kmap.reorder_children( parent
                         , children );

    return kmap::Result< std::string >{ "children sorted" };
}

auto swap_nodes( Uuid const& lhs
               , Uuid const& rhs )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();
    
    if( auto const r = kmap.swap_nodes( lhs , rhs )
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
    function( "cli", &kmap::binding::cli );
    function( "cli_on_key_up", &kmap::binding::cli_on_key_up );
    function( "complete_child_heading", &kmap::binding::complete_child_heading );
    function( "complete_cli", &kmap::binding::complete_cli );
    function( "complete_filesystem_path", &kmap::complete_filesystem_path );
    function( "complete_heading_path", &kmap::binding::complete_heading_path );
    function( "complete_heading_path_from", &kmap::binding::complete_heading_path_from );
    function( "count_ancestors", &kmap::binding::count_ancestors );
    function( "count_descendants", &kmap::binding::count_descendants );
    function( "create_alias", &kmap::binding::create_alias );
    function( "create_child", &kmap::binding::create_child );
    function( "delete_alias", &kmap::binding::delete_alias );
    function( "delete_children", &kmap::binding::delete_children );
    function( "delete_node", &kmap::binding::delete_node );
    function( "edit_body", &kmap::binding::edit_body );
    function( "execute_sql", &kmap::binding::execute_sql );
    function( "failure", &kmap::binding::make_failure );
    function( "fetch_body", &kmap::binding::fetch_body );
    function( "fetch_child", &kmap::binding::fetch_child );
    function( "fetch_descendant", &kmap::binding::fetch_descendant );
    function( "fetch_node", &kmap::binding::fetch_node ); // This fetches a single node and errors if ambiguous.
    // function( "fetch_nodes", &kmap::binding::fetch_nodes ); // This fetches one or more nodes.
    function( "fetch_or_create_node", &kmap::binding::fetch_or_create_node );
    function( "fetch_heading", &kmap::binding::fetch_heading );
    function( "fetch_parent", &kmap::binding::fetch_parent );
    function( "focus_network", &kmap::binding::focus_network );
    function( "fs_path_exists", &kmap::binding::fs_path_exists );
    function( "fetch_above", &kmap::binding::fetch_above );
    function( "fetch_below", &kmap::binding::fetch_below );
    function( "fetch_children", &kmap::binding::fetch_children );
    function( "is_alias", &kmap::binding::is_alias );
    function( "is_alias_pair", &kmap::binding::is_alias_pair );
    function( "is_ancestor", &kmap::binding::is_ancestor );
    function( "is_in_tree", &kmap::binding::is_in_tree );
    function( "is_lineal", &kmap::binding::is_lineal );
    function( "is_valid_heading", &kmap::binding::is_valid_heading );
    function( "is_valid_heading_path", &kmap::binding::is_valid_heading_path );
    function( "jump_in", &kmap::binding::jump_in );
    function( "jump_out", &kmap::binding::jump_out );
    function( "jump_stack", &kmap::binding::jump_stack );
    function( "load_state", &kmap::binding::load_state );
    function( "map_headings", &kmap::binding::map_headings );
    function( "move_body", &kmap::binding::move_body );
    function( "move_node", &kmap::binding::move_node );
    function( "on_leaving_editor", &kmap::binding::on_leaving_editor );
    function( "parse_cli", &kmap::binding::parse_cli );
    function( "resolve_alias", &kmap::binding::resolve_alias );
    function( "root_node", &kmap::binding::root_node );
    function( "run_unit_tests", &kmap::binding::run_unit_tests );
    function( "select_node", &kmap::binding::select_node );
    function( "selected_node", &kmap::binding::selected_node );
    function( "sort_children", &kmap::binding::sort_children );
    function( "success", &kmap::binding::make_success );
    function( "swap_nodes", &kmap::binding::swap_nodes );
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

    class_< kmap::binding::Cli >( "Cli" )
        .function( "notify_success", &kmap::binding::Cli::notify_success )
        .function( "notify_failure", &kmap::binding::Cli::notify_failure )
        .function( "reset_all_preregistered", &kmap::binding::Cli::reset_all_preregistered )
        ;
    class_< kmap::binding::JumpStack >( "JumpStack" )
        .function( "jump_in", &kmap::binding::JumpStack::jump_in )
        ;
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
    KMAP_BIND_RESULT( std::set<uint8_t> );
    KMAP_BIND_RESULT( std::set<uint16_t> );
    KMAP_BIND_RESULT( std::set<uint32_t> );
    KMAP_BIND_RESULT( std::set<uint64_t> );
    KMAP_BIND_RESULT( std::set<Uuid> );
    KMAP_BIND_RESULT( std::string );
    KMAP_BIND_RESULT( std::vector<uint16_t> );
    KMAP_BIND_RESULT( std::vector<uint32_t> );
    KMAP_BIND_RESULT( std::vector<uint64_t> );
    KMAP_BIND_RESULT( std::vector<uint8_t> );
    KMAP_BIND_RESULT( std::vector<Uuid> );
    KMAP_BIND_RESULT( uint16_t );
    KMAP_BIND_RESULT( uint32_t );
    KMAP_BIND_RESULT( uint64_t );
    KMAP_BIND_RESULT( uint8_t );
    class_< kmap::binding::Result< void > >( "Result<void>" )
        .function( "error_message", &kmap::binding::Result< void >::error_message )
        .function( "has_error", &kmap::binding::Result< void >::has_error )
        .function( "has_value", &kmap::binding::Result< void >::has_value )
        ;
}
