/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <cmd/command.hpp>

#include <cmd/parser.hpp>
#include <com/cmd/command.hpp>
#include <com/network/network.hpp>
#include <common.hpp>
#include <contract.hpp>
#include <emcc_bindings.hpp>
#include <error/master.hpp>
#include <error/network.hpp>
#include <io.hpp>
#include <kmap.hpp>
#include <path.hpp>
#include <path/act/order.hpp>
#include <path/node_view.hpp>
#include <util/result.hpp>
#include <util/script/script.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <range/v3/action/join.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <sstream>

using namespace ranges;

namespace kmap::cmd {

auto is_general_command( Kmap const& kmap
                       , Uuid const& id )
    -> bool
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

    auto rv = bool{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( view2::cmd::command_root
      | act2::exists( kmap ) )
    {
        rv = view::make( id )
           | view::desc( view::PredFn{ [ &kmap, &nw, &id ]( auto const& node )
                                       {
                                           return nw->distance( id, node ) == 1
                                               && is_particular_command( kmap, node );
                                       } } )
           | view::exists( kmap );
    }

    return rv;
}

auto is_particular_command( Kmap const& kmap
                          , Uuid const& id )
    -> bool
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

    auto rv = bool{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( auto const cmd_root = view2::cmd::command_root
                            | act2::fetch_node( kmap )
      ; cmd_root )
    {
        rv = nw->is_lineal( cmd_root.value(), id )
          && nw->is_child( id
                         , "description"
                         , "argument"
                         , "action" );
    }

    return rv;
}

auto is_argument( Kmap const& km
                , Uuid const& id )
    -> bool
{
    KM_RESULT_PROLOG();

    auto rv = bool{};
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    if( auto const cmd_root = view2::cmd::argument_root
                            | act2::fetch_node( km )
      ; cmd_root )
    {
        rv = nw->is_lineal( cmd_root.value(), id )
          && nw->is_child( id
                         , "description"
                         , "guard"
                         , "completion" );
    }

    return rv;
}

auto to_command_name( Uuid const& id )
    -> std::string
{
    auto const sid = to_string( id );
    auto const cid = sid
                   | views::remove( '-' )
                   | to< std::string >();
    auto const fn = fmt::format( "cmd_{}"
                               , cid );

    return fn;
}

auto execute_kscript( Kmap& kmap 
                    , std::string const& code )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "code", code );

    auto rv = result::make_result< void >();
    auto ss = std::stringstream{ code };

    KTRY( util::load_script( kmap, ss ) );

    rv = outcome::success();

    return rv;
}

auto execute_javascript( Uuid const& node
                       , std::string_view const body
                       , StringVec const& args )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", node );
        KM_RESULT_PUSH( "body", std::string{ body } );

    auto rv = result::make_result< void >();
    auto const stringed = args
                        | views::transform( []( auto const& arg ){ return io::format( "'{}'", boost::replace_all_copy( arg, "'", "\\'" ) ); } )
                        | to< StringVec >();
    auto const csep = stringed
                    | views::join( ',' )
                    | to< std::string >();

#if KMAP_LOG && 0
    io::print( "execute_javascript.args: {}\n", csep );
#endif

#if !KMAP_NATIVE
    auto const pp_body = KTRY( js::preprocess( std::string{ body } ) );

    KTRY( js::eval_void( fmt::format( "const args = to_VectorString( [{}] );\n{}", csep, pp_body ) ) );
#endif // !KMAP_NATIVE
    
    rv = outcome::success();

    return rv;
}

auto execute_body( Kmap& kmap
                 , Uuid const& node
                 , StringVec const& args )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", node );

    auto rv = result::make_result< void >();
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( anchor::node( node ) | act2::exists( kmap ) );
        })
    ;

    auto const body = KTRY( nw->fetch_body( node ) );
    auto const code = KTRY( parser::parse_body_code( body ) );

    KTRY( boost::apply_visitor( [ & ]( auto const& e ) -> Result< void >
                              {
                                  using T = std::decay_t< decltype( e ) >;

                                  if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                                  {
                                      KTRY( execute_kscript( kmap, e.code ) );
                                  }
                                  else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                                  {
                                      KTRY( execute_javascript( node, e.code, args ) );
                                  }
                                  else
                                  {
                                      static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                  }

                                  return outcome::success();
                              }
                              , code ) );

    rv = outcome::success();
    
    return rv;
}

// TODO: The fact that kmap is const, but executing the script could result in non-const actions is concerning to me.
auto evaluate_guard( Kmap const& kmap
                   , Uuid const& guard_node
                   , std::string const& arg )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", guard_node );
        KM_RESULT_PUSH_NODE( "arg", arg );

    auto rv = result::make_result< void >();
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const body = KTRY( anchor::node( guard_node )
                          | view2::child( "action" )
                          | act2::fetch_body( kmap ) );
    auto const code = KTRY( cmd::parser::parse_body_code( body ) );

    KTRY( boost::apply_visitor( [ & ]( auto const& e ) -> Result< void >
                              {
                                  using T = std::decay_t< decltype( e ) >;

                                  if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                                  {
                                      rv = KMAP_MAKE_ERROR_MSG( error_code::command::kscript_unsupported, "kscript is not supported in a guard node" );
                                  }
                                  else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                                  {
                                      auto const escaped_arg = boost::replace_all_copy( arg, "'", "\\'" );

                                      KTRY( execute_javascript( guard_node
                                                              , e.code
                                                              , { escaped_arg } ) );
                                  }
                                  else
                                  {
                                      static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                  }

                                  return outcome::success();
                              }
                        , code ) );

    rv = outcome::success();

    return rv;
}

auto evaluate_completer( Kmap& kmap
                       , Uuid const& completer_node
                       , std::string const& arg )
    -> Result< StringVec >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", completer_node );

    auto rv = KMAP_MAKE_RESULT( StringVec );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const body = KMAP_TRY( nw->fetch_body( completer_node ) );
    auto const code = KMAP_TRY( cmd::parser::parse_body_code( body ) );

    boost::apply_visitor( [ & ]( auto const& e )
    {
        using T = std::decay_t< decltype( e ) >;

        if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
        {
            rv = KMAP_MAKE_ERROR( error_code::command::kscript_unsupported );
        }
        else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
        {
            #if !KMAP_NATIVE
            auto const fn_name = fmt::format( "fn_{}", format_heading( to_string( completer_node ) ) );

            if( auto const fn_created = js::publish_function( fn_name, { "arg" }, e.code )
                ; fn_created )
            {
                if( auto const crv = js::call< StringVec >( fn_name, arg )
                    ; crv )
                {
                    rv = crv.value();
                }
                else
                {
                    EM_ASM
                    (
                        throw "Unknown type returned from guard function; expected kmap.VectorString";
                    );
                }
            }
            else
            {
                rv = KMAP_MAKE_ERROR( error_code::command::fn_publication_failed );
            }
            #endif // !KMAP_NATIVE
        }
        else
        {
            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
        }
    }
    , code );

    return rv;
}

auto has_unconditional_arg( Kmap const& kmap
                          , Uuid const& cmd )
    -> bool
{
    KM_RESULT_PROLOG();

    auto rv = false;

    KMAP_ENSURE_EXCEPT( anchor::node( cmd ) | view2::cmd::command | act2::exists( kmap ) );

    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const uncond_src = KTRYE( view2::cmd::argument_root
                                 | view2::child( "unconditional" )
                                 | act2::fetch_node( kmap ) );
    auto const arg_nodes = anchor::node( cmd )
                         | view2::child( "argument" )
                         | view2::child // "description" node (parent)
                         | view2::child // actual arg alias
                         | view2::resolve
                         | act2::to_node_set( kmap );
    
    rv = arg_nodes.contains( uncond_src );

    return rv;
}

auto parse_args( Kmap& kmap
               , Uuid const& cmd_id
               , std::string const& arg )
    -> Result< StringVec >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "cmd", cmd_id );

    KMAP_ENSURE( anchor::node( cmd_id ) | view2::cmd::command | act2::exists( kmap ), error_code::command::not_general_command );
    KMAP_ENSURE( view2::cmd::argument_root | act2::exists( kmap ), error_code::common::uncategorized );

    auto rv = KMAP_MAKE_RESULT( StringVec );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );
    auto const arg_node = KTRY( anchor::node( cmd_id )
                              | view2::child( "argument" )
                              | act2::fetch_node( kmap ) );
    auto const arg_nodes = anchor::node( arg_node )
                         | view2::child
                         | view2::order
                         | act2::to_node_vec( kmap );
    auto const split_args = arg | views::split( ' ' ); // Note the lazy split. Important for large strings that are actually unconditional, for performance.

    if( arg_nodes.size() == 0 )
    {
        rv = StringVec{};
    }
    else if( has_unconditional_arg( kmap, cmd_id )  )
    {
        rv = StringVec{ arg };
    }
    else if( std::cmp_equal( distance( split_args ), arg_nodes.size() ) )
    {
        auto validated_args = StringVec{}; // TODO: Is this even of use?

        for( auto const [ index, sarg ] : split_args | views::enumerate )
        {
            BC_ASSERT( index < arg_nodes.size() );

            auto const arg_descr = arg_nodes[ index ]; // A descriptor node preceds the aliased argument.
            auto const arg_descr_children = nw->fetch_children( arg_descr );

            if( arg_descr_children.size() == 1 )
            {
                if( auto const arg_node = *arg_descr_children.begin()
                  ; is_argument( kmap, nw->resolve( arg_node ) ) )
                {
                    auto const guardn = KTRY( anchor::node( arg_node )
                                            | view2::child( "guard" )
                                            | act2::fetch_node( kmap ) );

                    // KTRY( evaluate_guard( kmap, validity_check, sarg | to< std::string >() ) );
                    KTRY( cmd::execute_body( kmap, guardn, split_args | ranges::to< StringVec >() ) );

                    // TODO: push arg into rv vector.
                    validated_args.emplace_back( sarg | to< std::string >() );

                    rv = validated_args;
                }
                else
                {
                    rv = KMAP_MAKE_ERROR( error_code::command::nonarg_node_found );
                }
            }
            else
            {
                rv = KMAP_MAKE_ERROR( error_code::command::invalid_arg ); // TODO: this should be more descriptive.

                break;
            }

        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::command::incorrect_arg_number
                                , fmt::format( "expected between [{},{}] arguments, got {}"
                                             , arg_nodes.size() // TODO: Need to determine optional args.
                                             , arg_nodes.size()
                                             , distance( split_args ) ) );
    }

    return rv;
}

auto fetch_params_ordered( Kmap& kmap
                         , Uuid const& cmd_id )
    -> Result< UuidVec >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "cmd", cmd_id );

    auto rv = KMAP_MAKE_RESULT( UuidVec );
    auto const nw = KTRY( kmap.fetch_component< com::Network >() );

    KMAP_ENSURE( nw->exists( cmd_id ), error_code::network::invalid_node );

    auto params = UuidVec{};
    auto const children = anchor::node( cmd_id )
                        | view2::child( "argument" )
                        | view2::child
                        | view2::order
                        | act2::to_node_vec( kmap );

    for( auto const& child : children )
    {
        auto const param = nw->fetch_children( child );

        KMAP_ENSURE( param.size() == 1, error_code::command::invalid_arg );

        params.emplace_back( *param.begin() );
    }

    rv = params;

    return rv;
}

auto execute_command( Kmap& kmap
                    , Uuid const& node
                    , std::string const& arg )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "node", node );
        KM_RESULT_PUSH( "arg", arg );

    auto rv = result::make_result< void >();

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( !val::global( to_command_name( cmd_id ).c_str() ).isUndefined() );
        })
    ;

    KMAP_ENSURE( anchor::node( node ) | view2::cmd::command | act2::exists( kmap ), error_code::command::not_general_command );

    auto const args = KTRY( parse_args( kmap, node, arg ) );
    auto const action = KTRY( anchor::node( node )
                            | view2::child( "action" )
                            | act2::fetch_node( kmap ) );

    KTRY( execute_body( kmap, action, args ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::cmd