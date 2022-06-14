/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "emcc_bindings.hpp"

#include "db/autosave.hpp"
#include "canvas.hpp"
#include "db.hpp"
#include "error/cli.hpp"
#include "error/js_iface.hpp"
#include "error/master.hpp"
#include "error/network.hpp"
#include "event/event.hpp"
#include "filesystem.hpp"
#include "io.hpp"
#include "jump_stack.hpp"
#include "kmap.hpp"
#include "option/option.hpp"
#include "tag/tag.hpp"
#include "task/task.hpp"
#include "test/master.hpp"
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

struct Autosave
{
    Kmap& kmap_;

    Autosave( Kmap& kmap )
        : kmap_{ kmap }
    {
    }
    
    auto interval()
        -> binding::Result< void >
    {
        return kmap_.autosave().interval();
    }

    auto install_event_outlet( std::string const& unit )
        -> binding::Result< void >
    {
        return kmap_.autosave().install_event_outlet( unit );
    }

    auto uninstall_event_outlet()
        -> binding::Result< void >
    {
        return kmap_.autosave().uninstall_event_outlet();
    }

    auto set_threshold( uint32_t const threshold )
        -> void
    {
        return kmap_.autosave().set_threshold( threshold );
    }
};

// Note: These class wrappers are needed for global types, as Embind doesn't take well to references nor pointers,
//       so the whole class ends up being copied; thus, I am providing simple wrappers to get copied.
// TODO: Move to canvas.cpp
struct Canvas
{
    Canvas( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto complete_path( std::string const& path ) const
        -> StringVec
    {
        return kmap_.canvas().complete_path( path );
    }

    auto fetch_base( Uuid const& pane ) const
        -> Result< float >
    {
        return kmap_.canvas().pane_base( pane );
    }

    auto fetch_orientation( Uuid const& pane ) const
        -> Result< kmap::Orientation >
    {
        return kmap_.canvas().pane_orientation( pane );
    }

    auto fetch_pane( std::string const& path ) const
        -> binding::Result< Uuid >
    {
        return kmap_.canvas().fetch_pane( path );
    }
    
    auto focus( Uuid const& pane )
        -> binding::Result< void >
    {
        return kmap_.canvas().focus( pane );
    }

    auto hide( Uuid const& pane )
        -> binding::Result< void >
    {
        return kmap_.canvas().hide( pane );
    }

    auto orient( Uuid const& pane
               , kmap::Orientation const orientation )
        -> binding::Result< void >
    {
        return kmap_.canvas().orient( pane, orientation );
    }

    auto rebase( Uuid const& pane
               , float const base )
        -> binding::Result< void >
    {
        return kmap_.canvas().rebase( pane, base );
    }

    auto reorient( Uuid const& pane )
        -> binding::Result< void >
    {
        return kmap_.canvas().reorient( pane );
    }

    auto reveal( Uuid const& pane )
        -> binding::Result< void >
    {
        return kmap_.canvas().reveal( pane );
    }

    auto subdivide( Uuid const& pane
                  , std::string const& heading
                  , std::string const& orientation
                  , float const base
                  , std::string const& elem_type )
        -> binding::Result< Uuid >
    {
        auto const parsed_orient = KMAP_TRY( from_string< Orientation >( orientation ) );

        return kmap_.canvas().subdivide( pane, heading, Division{ parsed_orient, base }, elem_type );
    }

    auto update_all_panes()
        -> binding::Result< void >
    {
        return kmap_.canvas().update_all_panes();
    }

    auto breadcrumb_pane() const -> Uuid { return kmap_.canvas().breadcrumb_pane(); } 
    auto breadcrumb_table_pane() const -> Uuid { return kmap_.canvas().breadcrumb_table_pane(); } 
    auto canvas_pane() const -> Uuid { return kmap_.canvas().canvas_pane(); } 
    auto cli_pane() const -> Uuid { return kmap_.canvas().cli_pane(); } 
    auto completion_overlay() const -> Uuid { return kmap_.canvas().completion_overlay(); }
    auto editor_pane() const -> Uuid { return kmap_.canvas().editor_pane(); } 
    auto network_pane() const -> Uuid { return kmap_.canvas().network_pane(); }
    auto preview_pane() const -> Uuid { return kmap_.canvas().preview_pane(); }
    auto text_area_pane() const -> Uuid { return kmap_.canvas().text_area_pane(); }
    auto workspace_pane() const -> Uuid { return kmap_.canvas().workspace_pane(); }

    Kmap& kmap_; 
};

struct Cli
{
    auto focus()
        -> void
    {
        auto& kmap = Singleton::instance();

        kmap.cli().focus();
    }
    auto on_key_down( int const key
                    , bool const is_ctrl
                    , bool const is_shift
                    , std::string const& text )
        -> Result< void >
    {
        auto& kmap = Singleton::instance();

        return kmap.cli().on_key_down( key
                                     , is_ctrl 
                                     , is_shift
                                     , text );
    }
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
        -> Result< void >
    {
        auto& kmap = Singleton::instance();

        return kmap.cli().reset_all_preregistered();
    }
};

struct Database
{
    Kmap& kmap_;

    Database( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto init_db_on_disk( std::string const& path )
        -> binding::Result< void >
    {
        return kmap_.database().init_db_on_disk( kmap_root_dir / path );
    }

    auto flush_delta_to_disk()
        -> binding::Result< void >
    {
        return kmap_.database().flush_delta_to_disk();
    }

    auto has_file_on_disk()
        -> bool
    {
        return kmap_.database().has_file_on_disk();
    }
};

struct EventStore
{
    Kmap& kmap_;

    EventStore( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto fire_event( std::vector< std::string > const& requisites )
        -> Result< void >
    {
        return kmap_.event_store().fire_event( requisites | ranges::to< std::set >() );
    }
    auto reset_transitions( std::vector< std::string > const& requisites )
        -> Result< void >
    {
        return kmap_.event_store().reset_transitions( requisites | ranges::to< std::set >() );
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

struct Network
{
    Network( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto center_viewport_node( Uuid const& node )
        -> void
    {
        kmap_.network().center_viewport_node( node );
    }

    auto select_node( Uuid const& node )
        -> binding::Result< Uuid >
    {
        return kmap_.network().select_node( node );
    }

    auto scale_viewport( float const scale )
        -> binding::Result< void >
    {
        return kmap_.network().scale_viewport( scale );
    }

    auto viewport_scale()
        -> float
    {
        return kmap_.network().viewport_scale();
    }

    auto underlying_js_network()
        -> emscripten::val
    {
        return *kmap_.network().underlying_js_network();
    }

    Kmap& kmap_;
};

struct OptionStore
{
    Kmap& kmap_;

    OptionStore( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto apply( std::string const& path )
        -> binding::Result< void >
    {
        return kmap_.option_store().apply( path );
    }

    auto apply_all()
        -> binding::Result< void >
    {
        return kmap_.option_store().apply_all();
    }

    auto update_value( std::string const& path
                     , float const& value )
        -> binding::Result< void >
    {
        return kmap_.option_store().update_value( path, fmt::format( "{:.2f}", value ) );
    }
};

struct TagStore
{
    Kmap& kmap_;

    TagStore( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto create_tag( std::string const& path )
        -> binding::Result< Uuid >
    {
        return kmap::create_tag( kmap_, path );
    }
    auto tag_node( Uuid const& node
                 , std::string const& path )
        -> binding::Result< Uuid >
    {
        return kmap::tag_node( kmap_, node, path );
    }
};

struct TaskStore
{
    Kmap& kmap_;

    TaskStore( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto create_task( std::string const& title )
        -> binding::Result< Uuid >
    {
        return kmap_.task_store().create_task( title );
    }
};

struct TextArea
{
    Kmap& kmap_;

    TextArea( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto focus_editor()
        -> void
    {
        kmap_.text_area().focus_editor( );
    }

    auto rebase_pane( float percent )
        -> void
    {
        kmap_.text_area().rebase_pane( percent );
    }

    auto rebase_preview_pane( float percent )
        -> void
    {
        kmap_.text_area().rebase_preview_pane( percent );
    }
    
    auto set_editor_text( std::string const& text )
        -> void
    {
        kmap_.text_area().set_editor_text( text );
    }

    auto show_editor()
        -> binding::Result< void >
    {
        return kmap_.text_area().show_editor();
    }

    auto show_preview( std::string const& body_text )
        -> binding::Result< void >
    {
        return kmap_.text_area().show_preview( body_text );
    }
};

auto autosave()
    -> binding::Autosave
{
    return Autosave{ Singleton::instance() };
}

auto canvas()
    -> binding::Canvas
{
    return binding::Canvas{ Singleton::instance() };
}

auto event_store()
    -> binding::EventStore
{
    return binding::EventStore{ Singleton::instance() };
}

auto network()
    -> binding::Network
{
    return binding::Network{ Singleton::instance() };
}

auto tag_store()
    -> binding::TagStore
{
    auto& kmap = Singleton::instance();

    return binding::TagStore{ kmap };
}

auto task_store()
    -> binding::TaskStore
{
    auto& kmap = Singleton::instance();

    return binding::TaskStore{ kmap };
}

auto text_area()
    -> binding::TextArea
{
    auto& kmap = Singleton::instance();

    return binding::TextArea{ kmap };
}

auto vector_string_from_int_ptr( uintptr_t vec )
    -> std::vector< std::string >*
{
    return reinterpret_cast< std::vector< std::string >* >( vec );
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
    
    return kmap::Result< uint32_t >{ view::make( node ) | view::ancestor | view::count( kmap )  };
}

auto count_descendants( Uuid const& node )
    -> binding::Result< uint32_t >
{
    auto const& kmap = Singleton::instance();

    return kmap::Result< uint32_t >{ view::make( node ) | view::desc | view::count( kmap ) };
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

auto database()
    -> binding::Database
{
    return binding::Database{ Singleton::instance() };
}

auto delete_alias( Uuid const& node )
    -> binding::Result< Uuid >
{
    auto& kmap = Singleton::instance();

    return kmap.erase_alias( node );
}

auto delete_children( Uuid const& parent )
    -> binding::Result< std::string > // TODO: Why is this returning std::string? Think it should be void.
{
    auto& kmap = Singleton::instance();
    auto const res = view::make( parent )
                   | view::child 
                   | view::erase_node( kmap );
    
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

    return kmap.erase_node( node );
}

auto focus_network()
    -> binding::Result< void >
{
    auto& kmap = Singleton::instance();
    
    return kmap.focus_network();
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

auto has_delta()
    -> bool 
{
    auto const& kmap = Singleton::instance();
    auto const& db = kmap.database();

    return db.has_delta();
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

// TODO: This is incorrect. A heading is not identical to a heading path.
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
        return kmap::Result< std::string >{ KMAP_MAKE_ERROR( error_code::common::uncategorized ) };
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
    -> binding::Result< void >
{
    auto& kmap = Singleton::instance();

    return kmap.move_body( src, dst );
}

auto on_leaving_editor()
    -> binding::Result< void >
{
    auto& kmap = Singleton::instance();
    
    return kmap.on_leaving_editor();
}

auto option_store()
    -> binding::OptionStore
{
    auto& kmap = Singleton::instance();

    return binding::OptionStore{ kmap };
}

auto parse_cli( std::string const& input )
    -> void
{
    auto& kmap = Singleton::instance();
    
    kmap.parse_cli( input );
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
    
    KMAP_TRY( kmap.update_body( node, content ) );

    // TODO: Return result from 'update_*' in case error occurred.
    return kmap::Result< std::string >{ "updated" };
}

auto update_heading( Uuid const& node
                   , std::string const& content )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();
    
    KMAP_TRY( kmap.update_heading( node, content ) );

    // TODO: Return result from 'update_*' in case error occurred.
    return kmap::Result< std::string >{ "updated" };
}

auto update_title( Uuid const& node
                 , std::string const& content )
    -> binding::Result< std::string >
{
    auto& kmap = Singleton::instance();

    KMAP_TRY( kmap.update_title( node, content ) );

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
    auto& tv = kmap.text_area();
    
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
    auto& kmap = Singleton::instance();
    auto children = kmap.fetch_children_ordered( parent );

    children |= actions::sort( [ & ]( auto const& lhs
                                    , auto const& rhs )
    {
        return kmap.fetch_heading( lhs ).value() < kmap.fetch_heading( rhs ).value();
    } );

    KMAP_TRY( kmap.reorder_children( parent, children ) );

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
    // function( "edit_body", &kmap::binding::edit_body );
    // function( "fetch_nodes", &kmap::binding::fetch_nodes ); // This fetches one or more nodes.
    function( "autosave", &kmap::binding::autosave );
    function( "canvas", &kmap::binding::canvas );
    function( "cli", &kmap::binding::cli );
    function( "complete_child_heading", &kmap::binding::complete_child_heading );
    function( "complete_filesystem_path", &kmap::complete_filesystem_path );
    function( "complete_heading_path", &kmap::binding::complete_heading_path );
    function( "complete_heading_path_from", &kmap::binding::complete_heading_path_from );
    function( "count_ancestors", &kmap::binding::count_ancestors );
    function( "count_descendants", &kmap::binding::count_descendants );
    function( "create_alias", &kmap::binding::create_alias );
    function( "create_child", &kmap::binding::create_child );
    function( "database", &kmap::binding::database );
    function( "delete_alias", &kmap::binding::delete_alias );
    function( "delete_children", &kmap::binding::delete_children );
    function( "delete_node", &kmap::binding::delete_node );
    function( "eval_failure", &kmap::binding::make_eval_failure );
    function( "eval_success", &kmap::binding::make_eval_success );
    function( "event_store", &kmap::binding::event_store );
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
    function( "focus_network", &kmap::binding::focus_network );
    function( "fs_path_exists", &kmap::binding::fs_path_exists );
    function( "has_delta", &kmap::binding::has_delta );
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
    function( "network", &kmap::binding::network );
    function( "on_leaving_editor", &kmap::binding::on_leaving_editor );
    function( "option_store", &kmap::binding::option_store );
    function( "parse_cli", &kmap::binding::parse_cli );
    function( "present_time", &kmap::binding::present_time );
    function( "std_exception_to_string", &kmap::binding::std_exception_to_string );
    function( "print_std_exception", &kmap::binding::print_std_exception );
    function( "resolve_alias", &kmap::binding::resolve_alias );
    function( "root_node", &kmap::binding::root_node );
    function( "run_unit_tests", &kmap::binding::run_unit_tests );
    function( "select_node", &kmap::binding::select_node );
    function( "selected_node", &kmap::binding::selected_node );
    function( "sort_children", &kmap::binding::sort_children );
    function( "success", &kmap::binding::make_success );
    function( "swap_nodes", &kmap::binding::swap_nodes );
    function( "tag_store", &kmap::binding::tag_store );
    function( "task_store", &kmap::binding::task_store );
    function( "text_area", &kmap::binding::text_area );
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

    class_< kmap::binding::Autosave >( "Autosave" )
        .function( "interval", &kmap::binding::Autosave::interval )
        .function( "install_event_outlet", &kmap::binding::Autosave::install_event_outlet )
        .function( "uninstall_event_outlet", &kmap::binding::Autosave::uninstall_event_outlet )
        .function( "set_threshold", &kmap::binding::Autosave::set_threshold )
        ;
    class_< kmap::binding::Canvas >( "Canvas" )
        .function( "breadcrumb_pane", &kmap::binding::Canvas::breadcrumb_pane )
        .function( "breadcrumb_table_pane", &kmap::binding::Canvas::breadcrumb_table_pane )
        .function( "canvas_pane", &kmap::binding::Canvas::canvas_pane )
        .function( "cli_pane", &kmap::binding::Canvas::cli_pane )
        .function( "complete_path", &kmap::binding::Canvas::complete_path )
        .function( "completion_overlay", &kmap::binding::Canvas::completion_overlay )
        .function( "editor_pane", &kmap::binding::Canvas::editor_pane )
        .function( "fetch_base", &kmap::binding::Canvas::fetch_base )
        .function( "fetch_orientation", &kmap::binding::Canvas::fetch_orientation )
        .function( "fetch_pane", &kmap::binding::Canvas::fetch_pane )
        .function( "focus", &kmap::binding::Canvas::focus )
        .function( "hide", &kmap::binding::Canvas::hide )
        .function( "network_pane", &kmap::binding::Canvas::network_pane )
        .function( "orient", &kmap::binding::Canvas::orient )
        .function( "preview_pane", &kmap::binding::Canvas::preview_pane )
        .function( "rebase", &kmap::binding::Canvas::rebase )
        .function( "reorient", &kmap::binding::Canvas::reorient )
        .function( "reveal", &kmap::binding::Canvas::reveal )
        .function( "text_area_pane", &kmap::binding::Canvas::text_area_pane )
        .function( "update_all_panes", &kmap::binding::Canvas::update_all_panes )
        .function( "workspace_pane", &kmap::binding::Canvas::workspace_pane )
        ;
    class_< kmap::binding::Cli >( "Cli" )
        .function( "focus", &kmap::binding::Cli::focus )
        .function( "notify_failure", &kmap::binding::Cli::notify_failure )
        .function( "notify_success", &kmap::binding::Cli::notify_success )
        .function( "on_key_down", &kmap::binding::Cli::on_key_down )
        .function( "reset_all_preregistered", &kmap::binding::Cli::reset_all_preregistered )
        ;
    class_< kmap::binding::Database >( "Database" )
        .function( "init_db_on_disk", &kmap::binding::Database::init_db_on_disk )
        .function( "flush_delta_to_disk", &kmap::binding::Database::flush_delta_to_disk )
        .function( "has_file_on_disk", &kmap::binding::Database::has_file_on_disk )
        ;
    class_< kmap::binding::EventStore >( "EventStore" )
        .function( "fire_event", &kmap::binding::EventStore::fire_event )
        .function( "reset_transitions", &kmap::binding::EventStore::reset_transitions )
        ;
    class_< kmap::binding::JumpStack >( "JumpStack" )
        .function( "jump_in", &kmap::binding::JumpStack::jump_in )
        ;
    class_< kmap::binding::Network >( "Network" )
        .function( "center_viewport_node", &kmap::binding::Network::center_viewport_node )
        .function( "scale_viewport", &kmap::binding::Network::scale_viewport )
        .function( "select_node", &kmap::binding::Network::select_node )
        .function( "underlying_js_network", &kmap::binding::Network::underlying_js_network )
        .function( "viewport_scale", &kmap::binding::Network::viewport_scale )
        ;
    class_< kmap::binding::OptionStore >( "OptionStore" )
        .function( "apply", &kmap::binding::OptionStore::apply )
        .function( "apply_all", &kmap::binding::OptionStore::apply_all )
        .function( "update_value", &kmap::binding::OptionStore::update_value )
        ;
    class_< kmap::binding::TagStore >( "TagStore" )
        .function( "create_tag", &kmap::binding::TagStore::create_tag )
        .function( "tag_node", &kmap::binding::TagStore::tag_node )
        ;
    class_< kmap::binding::TaskStore >( "TaskStore" )
        .function( "create_task", &kmap::binding::TaskStore::create_task )
        ;
    class_< kmap::binding::TextArea >( "TextArea" )
        .function( "focus_editor", &kmap::binding::TextArea::focus_editor )
        .function( "rebase_pane", &kmap::binding::TextArea::rebase_pane )
        .function( "rebase_preview_pane", &kmap::binding::TextArea::rebase_preview_pane )
        .function( "set_editor_text", &kmap::binding::TextArea::set_editor_text )
        .function( "show_editor", &kmap::binding::TextArea::show_editor )
        .function( "show_preview", &kmap::binding::TextArea::show_preview )
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
    KMAP_BIND_RESULT( double );
    KMAP_BIND_RESULT( float );
    KMAP_BIND_RESULT( kmap::Orientation );
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

    enum_< kmap::Orientation >( "Orientation" )
        .value( "horizontal", kmap::Orientation::horizontal )
        .value( "vertical", kmap::Orientation::vertical )
        ;
}
