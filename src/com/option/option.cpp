/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "option.hpp"

#include "cmd/command.hpp"
#include "cmd/parser.hpp"
#include "com/cmd/command.hpp"
#include "com/network/network.hpp"
#include "contract.hpp"
#include "emcc_bindings.hpp"
#include "error/master.hpp"
#include "error/parser.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/node_view.hpp"

#include <boost/variant.hpp>

#include <emscripten.h>

namespace kmap::com {

OptionStore::OptionStore( Kmap& kmap
                        , std::set< std::string > const& requisites
                        , std::string const& description )
    : Component{ kmap, requisites, description }
{
}

auto OptionStore::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto OptionStore::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    rv = outcome::success();

    return rv;
}

auto OptionStore::option_root()
    -> Uuid
{
    auto& km = kmap_inst();

    return KTRYE( view::abs_root
                | view::direct_desc( "meta.setting.option" )
                | view::fetch_or_create_node( km ) );
}

// TODO: Should install_option() reject pre-existing headings, to avoid copy-paste mistakes, and use update_option() instead?
auto OptionStore::install_option( Heading const& heading
                                , std::string const& descr
                                , std::string const& value 
                                , std::string const& action  )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    KMAP_ENSURE( js::lint( action ), error_code::js::lint_failed );

    auto const action_body = fmt::format( "```javascript\n{}\n```", action );

    KMAP_ENSURE( cmd::parser::parse_body_code( action_body ), error_code::parser::parse_failed );

    auto const vopt = view::make( option_root() )
                    | view::direct_desc( heading );
    auto const nw = KTRY( fetch_component< com::Network >() );

    KTRY( nw->update_body( KTRY( vopt | view::direct_desc( "description" ) | view::fetch_or_create_node( km ) )
                         , descr ) );
    KTRY( nw->update_body( KTRY( vopt | view::direct_desc( "value" ) | view::fetch_or_create_node( km ) )
                         , value ) );
    KTRY( nw->update_body( KTRY( vopt | view::direct_desc( "action" ) | view::fetch_or_create_node( km ) )
                         , action_body ) );

    rv = KTRY( vopt | view::fetch_node( km ) );

    return rv;
}

auto OptionStore::is_option( Uuid const& option )
    -> bool
{
    auto& km = kmap_inst();
    auto const in_tree = view::make( option ) 
                       | view::ancestor( option_root() )
                       | view::exists( km );
    auto const has_structure = ( view::make( option )
                               | view::child( view::all_of( "description", "value", "action" ) )
                               | view::count( km ) ) == 3;

    return in_tree && has_structure;
}

auto OptionStore::uninstall_option( Heading const& heading )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const node = KTRY( view::make( option_root() )
                          | view::direct_desc( heading )
                          | view::fetch_node( km ) );

    KTRY( uninstall_option( node ) );

    rv = outcome::success();

    return rv;
}

auto OptionStore::uninstall_option( Uuid const& option )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();

    KMAP_ENSURE( view::make( option ) | view::exists( km ), error_code::network::invalid_node );
    KMAP_ENSURE( is_option( option ), error_code::network::invalid_node );

    rv = outcome::success();

    return rv;
}

auto OptionStore::update_value( Heading const& heading
                              , std::string const& value )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const action = KTRY( view::make( option_root() )
                            | view::direct_desc( heading )
                            | view::child( "value" )
                            | view::fetch_node( km ) );

    KTRY( nw->update_body( action, value ) );

    rv = outcome::success();

    return rv;
}

auto OptionStore::apply( Uuid const& option )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( is_option( option ), error_code::network::invalid_node );


    // auto const action = KTRY( view::make_view( kmap_, option ) | view::child( "action" ) | view::try_to_single );
    auto const nw = KTRY( fetch_component< com::Network >() );
    fmt::print( "option: {}\n", KTRYE( nw->fetch_heading( option ) ) );
    auto const action = KTRY( nw->fetch_child( option, "action" ) );
    // auto const action_body = KTRY( action.fetch_body() );
    auto const action_body = KTRY( nw->fetch_body( action ) );
    auto const code = cmd::parser::parse_body_code( action_body ).value();
    auto const value_node = KTRY( nw->fetch_child( option, "value" ) );
    auto const value_body = KTRY( nw->fetch_body( value_node ) );
    // TODO: Use similar technique to cmd action to expose "option_value" automatically via fn arg.
    // fmt::print( "option_store::apply: {}\n", km.absolute_path_flat( option ) );
    boost::apply_visitor( [ & ]( auto const& e ) // Note: spirit::x3::variant uses boost::variant which is not compatible with std::visit.
                            {
                                using T = std::decay_t< decltype( e ) >;

                                if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                                {
                                    KMAP_THROW_EXCEPTION_MSG( "kscript is not supported in a guard node" );
                                }
                                else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                                {
                                    auto const opt_val = fmt::format( "let option_value = {};", value_body );

                                    KMAP_TRYE( js::eval_void( fmt::format( "{}\n{}", opt_val, e.code ) ) );
                                }
                                else
                                {
                                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                }
                            }
                        , code );
    
    rv = outcome::success();

    return rv;
}

auto OptionStore::apply( std::string const& path )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const target = KTRY( view::make( option_root() ) 
                            | view::direct_desc( path )
                            | view::fetch_node( km ) );

    KTRY( apply( target ) );

    rv = outcome::success();

    return rv;
}

auto OptionStore::apply_all()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto& km = kmap_inst();
    auto const root = option_root();
    auto const options = view::make( root )
                       | view::desc
                       | view::child( view::all_of( "description", "action" ) ) // Select nodes when children ("description", "action")
                       | view::parent
                       | view::to_node_set( km ); // Select parent of ("description", "action")

    for( auto const& option : options )
    {
        KTRY( apply( option ) );
    }

    rv = outcome::success();

    return rv;
}

namespace binding {

using namespace emscripten;

struct OptionStore
{
    Kmap& km;

    OptionStore( Kmap& kmap )
        : km{ kmap }
    {
    }

    auto apply( std::string const& path )
        -> kmap::binding::Result< void >
    {
        auto const ostore = KTRY( km.fetch_component< com::OptionStore >() );

        return ostore->apply( path );
    }

    auto apply_all()
        -> kmap::binding::Result< void >
    {
        auto const ostore = KTRY( km.fetch_component< com::OptionStore >() );

        return ostore->apply_all();
    }

    auto update_value( std::string const& path
                     , float const& value )
        -> kmap::binding::Result< void >
    {
        auto const ostore = KTRY( km.fetch_component< com::OptionStore >() );

        return ostore->update_value( path, fmt::format( "{:.2f}", value ) );
    }
};

auto option_store()
    -> com::binding::OptionStore
{
    return com::binding::OptionStore{ kmap::Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_option_store )
{
    function( "option_store", &kmap::com::binding::option_store );
    class_< kmap::com::binding::OptionStore >( "OptionStore" )
        .function( "apply", &kmap::com::binding::OptionStore::apply )
        .function( "apply_all", &kmap::com::binding::OptionStore::apply_all )
        .function( "update_value", &kmap::com::binding::OptionStore::update_value )
        ;
}

} // namespace binding

namespace {
namespace option_store_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::OptionStore
,   std::set({ "component_store"s, "root_node"s })
,   "option related functionality"
);

} // namespace option_store_def 
}

#if 0
namespace apply_options_def {

auto const guard_code =
R"%%%(```javascript
return kmap.success( 'success' );
```)%%%";
auto const action_code =
R"%%%(```javascript
kmap.option_store().apply_all();

return kmap.success( 'all options applied' );
```)%%%";

using Guard = com::Command::Guard;
using Argument = com::Command::Argument;

auto const description = "applies all options in /meta.option";
auto const arguments = std::vector< Argument >{};
auto const guard = Guard{ "unconditional", guard_code };
auto const action = action_code;

REGISTER_COMMAND
(
    apply.options
,   description 
,   arguments
,   guard
,   action
);

} // namespace apply_options_def
#endif // 0

} // namespace kmap::com


