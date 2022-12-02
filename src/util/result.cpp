/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "util/result.hpp"

#include "com/network/network.hpp"
#include "error/result.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/fetch_heading.hpp"
#include "path/node_view2.hpp"
#include "utility.hpp"

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

#if PROFILE_LOG || 1
static auto local_log_depth = 0u;
LocalLog::LocalLog( const char* function )
    : func_name{ function }
{
    ++local_log_depth;
}

LocalLog::~LocalLog()
{
    {
        if( local_log_depth > 0 )
        {
            --local_log_depth;
        }

        auto const end_time = std::chrono::system_clock::now();
        auto const runtime = std::chrono::duration< double >{ end_time - start_time };
        if( auto const count = runtime.count()
          ; count > 0.5 )
        {
            fmt::print( "[{}]{}: {}\n", local_log_depth, func_name, runtime.count() );
        }
    }
}
#endif // PROFILE_LOG

auto LocalLog::push( LocalLog::MultiValue const& mv )
    -> void
{
    // fmt::print( "LocalLog::push: {}\n", mv.key );

    kvs.emplace_back( mv );
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
            return result::value_or( ( view::make( node ) | act::fetch_heading( km ) ), "N/A" );
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
            return result::value_or( ( view2::abs_root | view2::desc( node ) | view2::abs_path_flat( km ) ), "N/A" );
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

auto to_string( std::vector< LocalLog::MultiValue > const& mvs )
    -> std::string
{
    auto ss = std::stringstream{};

    ss << "{";

    for( auto const& mv : mvs )
    {
        ss << "'"
           << mv.key
           << "'"
           << ":";
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

        ss << ",";

        // if( mv.values.empty() )
        // {
        //     ss << "'"
        //        << mv.name
        //        << "'";
        // }
        // else
        // {
        //     ss << "'"
        //        << mv.name
        //        << "'"
        //        << ":";
            
        //     if( mv.values.size() == 1 )
        //     {
        //         ss << to_string( mv.values );
        //     }
        //     else
        //     {
        //         ss << "{"
        //            << to_string( mv.values )
        //            << "}";
        //     }
            
        //     ss << ",";
        // }
    }

    ss << "}";

    return ss.str();
}

// auto to_string( LocalLog::MultiValue const& mv )
//     -> std::string
// {
//     if( mv.values.size() == 0 )
//     {
//         return fmt::format( "'{}'", mv.name );
//     }
//     else
//     {
//         auto ss = std::stringstream{};

//         ss << "{ '"
//            << mv.name
//            << "'"
//            << " : ";

//         for( auto const& nmv : mv.values )
//         {
//            ss << to_string( nmv )
//               << ",";
//         }

//         ss << " }";

//         return ss.str();
//     }
// }

auto to_string( LocalLog const& state )
    -> std::string
{
    auto rv = std::string{};
    auto ss = std::stringstream{};

    ss << to_string( state.kvs );

    rv = ss.str();

    if( !rv.empty() )
    {
        rv = js::beautify( rv );
    }

    return rv;
}

} // namespace kmap::result
