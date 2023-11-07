/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <util/result.hpp>

#include <attribute.hpp>
#include <com/network/network.hpp>
#include <error/result.hpp>
#include <kmap.hpp>
#include <path/act/fetch_heading.hpp>
#include <path/node_view2.hpp>
#include <util/log/xml.hpp>
#include <util/script/script.hpp>
#include <utility.hpp>

#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <variant>

namespace kmap::result {

// LocalLog::LocalLog( std::pair< std::string, std::string > const& p )
//     : log{ p }
// {
// }

// LocalLog::LocalLog( std::vector< std::pair< std::string, std::string > > const& v )
// {
//     log.insert( log.end(), v.begin(), v.end() );
// }

#if KMAP_LOG
LocalLog::LocalLog( const char* function
                  , const char* file
                  , unsigned line )
    : scoped_log_{ function, file, line }
{
}
#endif // KMAP_LOG

#if 0 // KMAP_LOG_PROFILE
LocalLog::~LocalLog()
{
    {
        auto const end_time = std::chrono::system_clock::now();
        auto const runtime = std::chrono::duration< double >{ end_time - start_time };
        if( auto const count = runtime.count()
          ; count > 0.001 )
        {
            fmt::print( "|{}|{}s|'{}'|{}|\n", local_log_depth, runtime.count(), func_name, to_string( kvs ) );
        }

        if( local_log_depth > 0 )
        {
            --local_log_depth;
        }
    }
}
#endif // KMAP_LOG_PROFILE

#if KMAP_LOG
LocalState::LocalState( const char* function
                      , const char* file
                      , unsigned line )
    : log{ function, file, line }
{
}
#endif // KMAP_LOG

auto LocalLog::push( LocalLog::MultiValue const& mv )
    -> void
{
#if KMAP_LOG
    auto const logging_enabled = KM_LOG_IS_ENABLED();

    KM_LOG_PAUSE_SCOPE(); // Must pause logging to avoid recursion, specifically for to_string().

    if( logging_enabled )
    {
        // TODO: print to xml file: <value key="{}">{}</value>
        // global_state.call_stack.push_val()
        // v. global_state.call_stack.push_fn()
        // TODO: Further, enable KM_RESULT_PROLOG_RV( rv ) - to declare a return value such that on exit, it can be logged as well.
        //       Tricky... I think the stored RV type would need to be an std::any, then check for the basic types, and ignore the others.
        if( kmap::util::log::Singleton::instance().flags.enable_call_stack )
        {
            auto csn = kmap::util::log::GlobalState::CallStack::Node{};
            auto const val = std::visit( [ & ]( auto const& e ){ return util::log::to_xml( e ); }, mv.value );

            csn.put( "<xmlattr>.key", mv.key );
            csn.put_child( "value", val );

            kmap::util::log::Singleton::instance().call_stack.add( "fvalue", csn );
        }
    }
#endif // KMAP_LOG

    kvs.emplace_back( mv );
}

auto LocalLog::values() const
    -> std::vector< MultiValue > const&
{
    return kvs;
}

auto dump_about( Kmap const& km
               , Uuid const& node )
    -> std::vector< LocalLog::MultiValue >
{
    auto const id = to_string( node );
    auto const heading = [ & ]() -> std::string
    {
        if( km.fetch_component< com::Network >().has_value() ) // Note: fetch_heading depends on this component.
        {
            return result::value_or( ( anchor::node( node ) | act2::fetch_heading( km ) ), "N/A" );
        }
        else
        {
            return "N/A";
        }
    }();
    auto const path = [ & ]() -> std::string
    {
        if( km.fetch_component< com::Network >().has_value() ) // Note: abs_root, desc depends on this component.
        {
            if( auto const apr = anchor::abs_root | view2::desc( node ) | act2::abs_path_flat( km )
              ; apr )
            {
                return apr.value();
            }
            else if( attr::is_in_attr_tree( km, node ) )
            {
                if( auto const attr_root = anchor::node( node ) | view2::root | act2::fetch_node( km )
                  ; attr_root )
                {
                    if( auto const attr_owner = attr::fetch_attr_owner( km, attr_root.value() )
                      ; attr_owner )
                    {
                        auto const owner_abspath = result::value_or( ( anchor::abs_root | view2::desc( attr_owner.value() ) | act2::abs_path_flat( km ) ), "N/A" );
                        auto const attr_abspath = result::value_or( ( anchor::node( attr_root.value() ) | view2::desc( node ) | act2::abs_path_flat( km ) ), "N/A" );

                        return fmt::format( "{}.$.{}", owner_abspath, attr_abspath );
                    }
                    else
                    {
                        return "N/A";
                    }
                }
                else
                {
                    return "N/A";
                }
            }
            else
            {
                return "N/A";
            }
        }
        else
        {
            return "N/A";
        }
    }();

    return { { "id", id }
           , { "heading", heading }
           , { "path", path } };
}

auto to_string( LocalLog::MultiValue const& mv )
    -> std::string
{
    auto ss = std::stringstream{};
    auto const dispatch = util::Dispatch
    {
        [ & ]( char const* arg )
        {
            ss << "'"
                << arg
                << "'";
        }
    ,   [ & ]( std::string const& arg )
        {
            ss << "'"
                << arg
                << "'";
        }
    ,   [ & ]( Uuid const& arg )
        {
            ss << to_string( result::dump_about( kmap::Singleton::instance(), arg ) );
        }
    ,   [ & ]( std::vector< LocalLog::MultiValue > const& arg )
        {
            ss << "{"
                << to_string( arg )
                << "}";
        }
    };

    std::visit( dispatch, mv.value );

    return ss.str();
}

auto to_string( std::vector< LocalLog::MultiValue > const& mvs )
    -> std::string
{
    auto ss = std::stringstream{};

    ss << "{";

    for( auto const& mv : mvs )
    {
        ss << to_string( mv );
    }

    ss << "}";

    return ss.str();
}

auto to_string( LocalLog const& state )
    -> std::string
{
    auto rv = std::string{};
    auto ss = std::stringstream{};

    ss << to_string( state.values() );

    rv = ss.str();

    if( !rv.empty() )
    {
        rv = util::js::beautify( rv );
    }

    return rv;
}

} // namespace kmap::result
