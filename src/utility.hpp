/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTILITY_HPP
#define KMAP_UTILITY_HPP

#include <common.hpp>
#include "error/node_manip.hpp"
#include "io.hpp"
// #include "path.hpp"
#include "stmt_prep.hpp"
#include "util/macro.hpp"

#include <boost/timer/timer.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/regex.hpp>
#include <dry-comparisons.hpp>

#include <string>
#include <vector>

#define KMAP_TIME_SCOPE( msg ) \
    fmt::print( "{}...\n", msg ); \
    boost::timer::auto_cpu_timer kmap_scope_timer( fmt::format( "{} done: %ws\n", msg ) );
#if KMAP_PROFILE || 0
    #define KMAP_PROFILE_SCOPE() \
            boost::timer::auto_cpu_timer KMAP_CONCAT( kmap_profile_scope_timer_, __LINE__ )( fmt::format( "{}|{}| done: %ws\n", __PRETTY_FUNCTION__, __LINE__ ) ); 
#else
    #define KMAP_PROFILE_SCOPE() ( void )0;
#endif // KMAP_PROFILE

namespace kmap {

using boost::uuids::to_string;
// Note: It appears Clang 10 can't handle rollbear::* at this time.
//       Update: It appears master branch _is_ cooperating with clang now.
//       In addition, including these in kmap appears to confuse the compiler
//       when compiling sqlpp11 query statements.

namespace util
{
    using rollbear::any_of;
    using rollbear::all_of;
    using rollbear::none_of;

    template< typename... Ts > struct Dispatch : Ts... { using Ts::operator()...; };
    template< typename... Ts > Dispatch( Ts... ) -> Dispatch< Ts... >; // Deducation guide.
}

namespace com
{
    class Database;
    class Network;
}

enum class Color
{
    white
,   black
,   red
,   orange
,   yellow
,   green
,   blue
,   indigo
,   violet
};

auto const color_level_map = std::vector< Color >
{
    Color::red
,   Color::orange
,   Color::yellow
,   Color::green
,   Color::blue
,   Color::indigo
,   Color::violet
};

auto const uuid_regex_pattern = boost::regex{ "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$" }; // Prefer boost::regex over std::regex, for performance. Long story... basically, standard made optimization std::regex ABI-breaking, so it remains unoptimized.

template< typename T >
auto from_string( std::string const& s )
    -> Result< T >;
template<>
auto from_string( std::string const& s )
    -> Result< bool >;
[[ nodiscard ]]
auto compress_resource( std::byte const* data
                      , size_t const size )
    -> std::vector< std::byte >;
auto configure_terminate()
    -> void;
[[ nodiscard ]]
auto decompress_resource( std::byte const* data
                        , size_t const data_size
                        , size_t const out_size )
    -> std::vector< std::byte >;
[[ nodiscard ]]
auto gen_uuid()
    -> Uuid;
[[ nodiscard ]]
auto gen_md5_uuid( FILE* fp )
    -> Uuid;
[[ nodiscard ]]
auto gen_md5_uuid( std::byte const* data
                 , size_t const size )
    -> Uuid; 
[[ nodiscard ]]
auto gen_uuid_string()
    -> std::string;
[[ nodiscard ]]
auto uuid_from_string( std::string const& suuid )
    -> Result< Uuid >;
[[ nodiscard ]]
auto gen_temp_db_name()
    -> std::string;
[[ nodiscard ]]
auto to_ordering_id( Uuid const& id )
    -> std::string;
[[ nodiscard ]]
auto to_uuids( HeadingPath const& path
             , com::Database& db
             , Uuid const& root )
    -> std::vector< Uuid >;
[[ nodiscard ]]
auto leaf( UuidPath const& path )
    -> Uuid;
[[ nodiscard ]]
auto mid( std::vector< Uuid > const& ids )
    -> Uuid;
[[ nodiscard ]]
auto longest_common_prefix( StringVec const& ss )
    -> std::string;
[[ nodiscard ]]
auto map_match_lengths( Heading const& from
                      , std::vector< Heading > const& to )
    -> std::vector< std::pair< uint32_t
                             , Heading > >;
[[ nodiscard ]]
auto match_length( std::string const& s
                 , std::string const& to )
    -> uint32_t;
[[ nodiscard ]]
auto fetch_first_invalid( Heading const& heading )
    -> Optional< uint32_t >;
[[ nodiscard ]]
auto is_valid_heading( Heading const& heading )
    -> bool;
[[ nodiscard ]]
auto is_valid_heading_path( std::string const& path )
    -> bool;
[[ nodiscard ]]
auto format_title( Heading const& heading )
    -> Title;
[[ nodiscard ]]
auto format_heading( Title const& title )
    -> Heading;
template< typename T >
[[ nodiscard ]]
auto to_optional( Result< T > const& r )
    -> Optional< T >
{
    if( r )
    {
        return Optional< T >{ r.value() };
    }
    else
    {
        return nullopt;
    }
}
[[ nodiscard ]]
auto to_string( Color const& c )
    -> std::string;
[[ nodiscard ]]
auto to_string( bool const b )
    -> std::string;
[[ nodiscard ]]
auto to_string_elaborated( Kmap const& kmap
                         , Uuid const node )
    -> std::string;
[[ nodiscard ]]
auto to_uint64( std::string const& s )
    -> Result< uint64_t >;
[[ nodiscard ]]
auto to_uint64( std::string const& s 
              , int const base )
    -> Result< uint64_t >;
[[ nodiscard ]]
auto to_uint64( Uuid const& id )
    -> Result< uint64_t >;
[[ nodiscard ]]
auto to_uuid( uint64_t const& id )
    -> Uuid;
[[ nodiscard ]]
auto to_uuid( std::string const& s )
    -> Result< Uuid >;
[[ nodiscard ]]
auto match_closest( Heading const& unknown
                  , std::vector< Heading > const& knowns )
    -> Heading;
[[ nodiscard ]]
auto xor_ids( Uuid const& lhs
            , Uuid const& rhs )
    -> Uuid;
[[ nodiscard ]]
auto make_edge_id( Uuid const& from
                 , Uuid const& to )
    -> Uuid;
[[ nodiscard ]]
auto url_to_heading( std::string const url )
    -> Heading;
[[ nodiscard ]]
auto present_time()
    -> uint64_t;
[[ nodiscard ]]
auto fetch_completions( std::string const& unknown
                      , StringVec const& knowns )
    -> StringVec;
[[ nodiscard ]]
auto file_exists( FsPath const& p ) // Use in lieu of fs::exists to work around MSYS lstat issue.
    -> bool;
auto merge_trees( com::Network& kmap
                , Uuid const& src 
                , Uuid const& dst )
    -> void;
auto merge_trees( StatementPreparer& stmts
                , Uuid const& src 
                , Uuid const& dst )
    -> void;
[[ nodiscard ]]
auto merge_ranges( std::set< uint64_t > const& values )
    -> std::set< std::pair< uint64_t, uint64_t > >;
auto print_stacktrace()
    -> void;
auto print_tree( Kmap const& kmap
               , Uuid const& root )
    -> Result< void >;
[[ nodiscard ]]
auto copy_body( com::Network& nw
              , Uuid const& src
              , Uuid const& dst )
    -> Result< void >;
[[ nodiscard ]]
auto move_body( com::Network& kmap
              , Uuid const& src
              , Uuid const& dst )
    -> Result< void >;
// TODO: There's no reason this shouldn't be a generic template. Nothing specific to UUids.
[[ nodiscard ]]
auto select_median_range( std::vector< Uuid > const& range
                        , Uuid const& median
                        , uint32_t const& max  )
    -> UuidVec;
[[ nodiscard ]]
auto select_median_range( std::vector< Uuid > const& range
                        , uint32_t const& max  )
    -> UuidVec;
[[ nodiscard ]]
auto fetch_siblings( Kmap const& kmap
                   , Uuid const& id )
    -> UuidSet;
[[ nodiscard ]]
auto fetch_siblings_ordered( Kmap const& kmap
                           , Uuid const& id )
    -> UuidVec;
[[ nodiscard ]]
auto fetch_siblings_inclusive( Kmap const& kmap
                             , Uuid const& id )
    -> UuidSet;
[[ nodiscard ]]
auto fetch_siblings_inclusive_ordered( Kmap const& kmap
                                     , Uuid const& id )
    -> UuidVec;
[[ nodiscard ]]
auto to_heading_path( Kmap const& kmap
                    , UuidVec const& lineage )
    -> StringVec;
[[ nodiscard ]]
auto to_heading_path_flat( Kmap const& kmap
                         , UuidVec const& lineage )
    -> Heading;
[[ nodiscard ]]
auto flatten( StringVec const& v
            , char const c )
    -> std::string;
[[ nodiscard ]]
auto flatten( HeadingPath const& path )
    -> Heading;
auto is_ordered( Kmap const& kmap
               , Uuid const& former 
               , Uuid const& latter )
    -> bool;
template< typename Target, typename... Args >
auto make( Args&&... args )
    -> Result< Target >
{
    return Target::make( std::forward< Args >( args )... );
}
auto replace_unescaped_char( std::string const& input
                           , char const c
                           , std::string rep )
    -> std::string;

} // namespace kmap

#endif // KMAP_UTILITY_HPP
