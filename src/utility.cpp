/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "utility.hpp"

#include "contract.hpp"
#include "com/database/db.hpp"
#include "com/filesystem/filesystem.hpp"
#include "com/network/network.hpp"
#include "error/master.hpp"
#include "error/node_manip.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "path/act/order.hpp"
#include "path/act/value_or.hpp"
#include "path/node_view.hpp"
#include <cmd/parser.hpp>
#include <js_iface.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/filesystem.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <catch2/catch_test_macros.hpp>
#include <emscripten.h>
#include <emscripten/val.h>
#include <openssl/md5.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/transform.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/find_if_not.hpp>
#include <range/v3/algorithm/replace.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/replace.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

using namespace kmap::result;
using namespace ranges;
namespace fs = boost::filesystem;

namespace kmap {

template<>
auto from_string( std::string const& s )
    -> Result< bool >
{
    auto rv = KMAP_MAKE_RESULT_EC( bool, error_code::common::conversion_failed );
    
    if( s == "true" )
    {
        rv = true;
    }
    else if( s == "false" )
    {
        rv = false;
    }

    return rv;
}
 
auto compress_resource( std::byte const* data
                      , size_t const size )
    -> std::vector< std::byte >
{
    auto rv = std::vector< std::byte >{};

    rv.resize( size );

    auto stream = [ & ]
    {
        auto zsm = z_stream{};

        zsm.zalloc = Z_NULL;
        zsm.zfree = Z_NULL;
        zsm.opaque = Z_NULL;
        zsm.avail_in = size;
        zsm.next_in = reinterpret_cast< Bytef z_const* >( data );
        zsm.avail_out = rv.size();
        zsm.next_out = reinterpret_cast< Bytef* >( rv.data() );

        return zsm;
    }();

    // TODO: Return failure instead of assertion.
    auto const compression_level = 8; // According to the benchmarks I've seen, level 8 gives the best compression ratio for our use without being egregiously slow.
    if( auto const succ = deflateInit( &stream, compression_level )
      ; succ < 0 )
    {
        KMAP_THROW_EXCEPTION_MSG( "deflateInit failed" );
    }
    if( auto const succ = deflate( &stream, Z_FINISH )
      ; succ < 0 )
    {
        KMAP_THROW_EXCEPTION_MSG( "deflate failed" );
    }
    if( auto const succ = deflateEnd( &stream )
      ; succ < 0 )
    {
        KMAP_THROW_EXCEPTION_MSG( "deflateEnd failed" );
    }

    rv.resize( stream.total_out );
    rv.shrink_to_fit();

    return rv;
}

auto configure_terminate()
    -> void
{
    std::set_terminate( []()
    {
        fmt::print( stderr
                  , "[terminate_handler]: std::terminate called!\n" );

        auto eptr = std::current_exception();

        if( eptr )
        {
            try
            {
                std::rethrow_exception( eptr );
            } 
            catch( std::exception& e )
            {
                fmt::print( stderr
                          , "[terminate_handler]: std::terminate called: {}\n"
                          , e.what() );

                abort();
            }
        }
        else
        {
            fmt::print( "std::terminate called\n" );

            abort();
        }
    } );
}

auto decompress_resource( std::byte const* data
                        , size_t const data_size
                        , size_t const out_size )
    -> std::vector< std::byte >
{
    auto rv = std::vector< std::byte >{};

    rv.resize( out_size );

    auto stream = [ & ]
    {
        auto zsm = z_stream{};

        zsm.zalloc = Z_NULL;
        zsm.zfree = Z_NULL;
        zsm.opaque = Z_NULL;
        zsm.avail_in = data_size;
        zsm.next_in = reinterpret_cast< Bytef z_const* >( data );
        zsm.avail_out = rv.size();
        zsm.next_out = reinterpret_cast< Bytef* >( rv.data() );

        return zsm;
    }();

    // TODO: Return failure instead of assertion.
    assert( inflateInit( &stream ) >= 0 );
    assert( inflate( &stream
                   , Z_NO_FLUSH ) >= 0 );
    assert( inflateEnd( &stream ) >= 0 );

    rv.resize( stream.total_out );
    rv.shrink_to_fit();

    return rv;
}

auto gen_uuid()
    -> Uuid
{
    auto rv = Uuid{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( !rv.is_nil() );
        })
    ;

    using boost::uuids::random_generator;

    rv = random_generator{}();

    return rv;
}

// TODO: Use fstream instead?
// TODO: Deprecate. Firstly, openssl has these annoying compilation warnings about md5 being deprecated that I can't turn off, so I'm temporarily disabling,
//       as it's not in use at the moment anyway. Secondly, I should probably use a less collision prone hash, such as SHA1 or SHA256. It's probably overkill, but also
//       future proof. As it's larger than a UUID, I couldn't use it for the resource node ID; however, I could simply make the hash part of the node's attributes, which
//       are arbitrary sized. This is probably the way to go.
auto gen_md5_uuid( FILE* fp )
    -> Uuid
{
    auto rv = Uuid{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( MD5_DIGEST_LENGTH == sizeof( Uuid::data ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !rv.is_nil() );
        })
    ;

#if 0
    using std::array;
    using InBuf = array< char
                       , 512 >; 

    auto inbuf = InBuf{};
    auto ctx = MD5_CTX{};
    auto bytes = fread( inbuf.data()
                      , 1
                      , 512
                      , fp );

    MD5_Init( &ctx );

    while( bytes > 0 )
    {
        MD5_Update( &ctx
                  , inbuf.data()
                  , bytes );

        bytes = fread( inbuf.data()
                     , 1
                     , 512
                     , fp );
    }

    MD5_Final( rv.data
             , &ctx );
#endif

    return rv;
}

auto gen_md5_uuid( std::byte const* data
                 , size_t const size )
    -> Uuid
{
    auto rv = Uuid{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( MD5_DIGEST_LENGTH == sizeof( Uuid::data ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !rv.is_nil() );
        })
    ;

#if 0
    auto const chunk_size = size_t{ 512 };
    auto const dend = data + size;
    auto next = [ chunk_size
                , dend ]
                ( std::byte const* pos )
    {
        assert( pos <= dend );

        if( pos == dend )
        {
            return dend;
                   
        }
        else if( pos + chunk_size > dend )
        {
            return dend;
        }
        else
        {
            return std::next( pos
                            , chunk_size );
                   
        }
    };
    auto pos = data;
    auto next_pos = next( pos );
    auto count = distance( pos
                         , next_pos );
    auto ctx = MD5_CTX{};

    MD5_Init( &ctx );

    while( count > 0 )
    {
        MD5_Update( &ctx
                  , pos
                  , count );
        
        pos = next_pos;
        next_pos = next( pos );
        count = distance( pos
                        , next_pos );
    }

    MD5_Final( rv.data
             , &ctx );
#endif

    return rv;
}

auto gen_uuid_string()
    -> std::string
{
    return to_string( gen_uuid() );
}

// TODO: rename to to_uuid()?
auto uuid_from_string( std::string const& suuid )
    -> Result< Uuid >
{
    using boost::uuids::string_generator;

    try
    {
        return string_generator{}( suuid );
    }
    catch( std::exception const& e )
    {
        io::print( "exception: {}\n", e.what() );
        // std::cerr << e.what() << '\n'; TODO: Add to EC payload?
        return KMAP_MAKE_ERROR( error_code::node::invalid_uuid );
    }
}

auto gen_temp_db_name()
    -> std::string
{
    return ".tmp." + gen_uuid_string() + ".kmap";
}

auto to_ordering_id( Uuid const& id )
    -> std::string
{
    auto const sid = to_string( id );

    return sid
         | views::take( 8 )
         | to< std::string >();
}

auto to_uuids( HeadingPath const& path
             , com::Database& db
             , Uuid const& root )
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};
    auto parent = root;

    for( auto const& e : path )
    {
        auto const ou = db.fetch_child( parent, e );

        if( !ou )
        {
            // TODO: report error.
            assert( false );
        }

        rv.emplace_back( parent );
        parent = ou.value();
    }

    return rv;
}

auto leaf( UuidPath const& path )
    -> Uuid
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( path.size() > 0 );
        })
    ;

    return path.back();
}

auto mid( std::vector< Uuid > const& ids )
    -> Uuid
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( ids.size() > 0 );
        })
    ;

    using std::ceil;

    auto const offset = static_cast< uint32_t >( ceil( ids.size() / 2.0 ) );

    return ids[ offset - 1 ];
}

auto match_length( std::string const& s
                 , std::string const& to )
    -> uint32_t
{
    using std::min;

    auto rv = uint32_t{};

    for( auto const i : views::indices( min( s.size()
                                          , to.size() ) ) )
    {
        if( s[ i ] == to[ i ] )
        {
            ++rv;
        }
        else
        {
            break;
        }
    }

    return rv;
}

auto is_valid_heading_char( char const c )
    -> bool
{
    if( std::isalnum( c ) )
    {
        if( std::isalpha( c ) )
        {
            if( std::islower( c ) )
            {
                return true;
            }
        }
        else
        {
            return true;
        }
    }
    else if( c == '_' )
    {
        return true;
    }

    return false;
}

// TODO: Replace with Boost.Spirit
auto fetch_first_invalid( Heading const& heading )
    -> Optional< uint32_t >
{
    auto rv = Optional< uint32_t >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( *rv < heading.size() );
            }
        })
    ;

    auto const it = find_if_not( heading
                               , is_valid_heading_char );

    if( it != heading.end() )
    {
        rv = distance( heading.begin()
                     , it );
    }

    return rv;
}

// TODO: Should not rather constraints on Heading be expressed in the constructor
// of the Heading class? This way, any place a heading is passed, it is known to be
// in a good, valid state.
auto is_valid_heading( Heading const& heading )
    -> bool
{
    return !fetch_first_invalid( heading );
}

SCENARIO( "is_valid_heading", "[network]" )
{
    REQUIRE( is_valid_heading( "1" ) );
    REQUIRE( is_valid_heading( "11" ) );
    REQUIRE( is_valid_heading( "_" ) );
    REQUIRE( is_valid_heading( "1_" ) );
    REQUIRE( is_valid_heading( "_1" ) );
    REQUIRE( is_valid_heading( "x" ) );
    REQUIRE( is_valid_heading( "xyz" ) );
}

// TODO: Belongs in path.cpp, not common utility.
// TODO: Might this not be better to call the path parser (Boost.Spirit (probably doesn't exist yet...)), and check that way?
auto is_valid_heading_path( std::string const& path )
    -> bool
{
    auto is_valid = []( auto const c )
    {
        return is_valid_heading_char( c )
            || c == ',' // back
            || c == '.' // forward
            || c == '\'' // disambiguator 
            || c == '/'; // root
    };

    return end( path ) == find_if_not( path, is_valid );
}

auto format_title( Heading const& heading )
    -> Title
{
    BC_CONTRACT()
        BC_POST([ & ]
        {
        })
    ;

    // TODO: Figure out how to do this all elegantly with one range statement.
    auto rv = heading 
            | views::transform( []( auto const& e ){ return tolower( e ); } )
            | views::split( '_' )
            | views::transform( []( auto const& e ){ return to< std::string >( e ); } )
            | to< StringVec >();

    // Capitalize first of every word.
    for( auto&& e : rv )
    {
        e[ 0 ] = toupper( e[ 0 ] );
    }

    return rv 
         | views::join( ' ' )
         | to< Title >();
}

auto format_heading( Title const& title )
    -> Heading
{
    auto rv = Heading{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( is_valid_heading( rv ) );
        })
    ;

    auto heading = title;

    boost::to_lower( heading );

    for( auto const [ index, c ] : views::enumerate( heading ) )
    {
        if( !is_valid_heading_char( c ) )
        {
            heading[ index ] = '_';
        }
    }

    rv = heading;

    return rv;
}

auto flatten( HeadingPath const& path )
    -> Heading
{
    return path
         | views::join( '.' )
         | to< Heading >();
}

auto to_string( Color const& c )
    -> std::string
{
    switch( c )
    {
    case Color::white: return "white";
    case Color::black: return "black";
    case Color::red: return "red";
    case Color::orange: return "orange";
    case Color::yellow: return "yellow";
    case Color::green: return "green";
    case Color::blue: return "blue";
    case Color::indigo: return "indigo";
    case Color::violet: return "violet";
    default: assert( false );
    }

    return ""; // Avoid compiler warning.
}

auto to_string( bool const b )
    -> std::string
{
    if( !b )
    {
        return "false";
    }
    else
    {
        return "true";
    }
}

auto to_string_elaborated( Kmap const& km
                         , Uuid const node )
    -> std::string
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( km.fetch_component< com::Network >() );
    auto const sid = to_string( node );
    auto const heading = nw->fetch_heading( node ) | act::value_or( std::string{ "n/a" } );
    auto const path = [ & ]() -> std::string
    { 
        if( nw->exists( node ) )
        {
            return KTRYE( absolute_path_flat( km, node ) );
        }
        else
        {
            return "n/a";
        }
    }();

    return fmt::format( "[ id: '{}', h: '{}', p: '{}' ]", sid, heading, path );
}

auto to_uint64( std::string const& s
              , int const base )
    -> Result< uint64_t >
{
    static_assert( sizeof( decltype( std::stoull( "", 0, base ) ) ) == sizeof( uint64_t ) );

    auto rv = KMAP_MAKE_RESULT( uint64_t );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                // BC_ASSERT( to_string( rv.value() ) == s ); // TODO.
            }
        })
    ;

    try
    {
        rv = std::stoull( s, 0, base );
    }
    catch( std::exception& e )
    {
        // TODO: propagate payload?
        io::print( stderr
                 , "to_uint64 failed: {}"
                 , e.what() );

        rv = KMAP_MAKE_ERROR( error_code::common::conversion_failed ); // TODO: numeric::conversion_failed?
    }

    return rv;
}

auto to_uint64( std::string const& s )
    -> Result< uint64_t >
{
    return to_uint64( s, 10 );
}

// TODO: 'id' Should probably be a integral constant (C++20)
auto to_uint64( Uuid const& id )
    -> Result< uint64_t >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "id", id );

    auto rv = KMAP_MAKE_RESULT( uint64_t );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( to_uuid( rv.value() ) == id );
            }
        })
    ;

    static_assert( sizeof( id.data ) == sizeof( uint64_t ) * 2 );

    auto tv = uint64_t{};

    std::copy( std::begin( id.data ) + sizeof( uint64_t ), std::end( id.data )
             , reinterpret_cast< char* >( &tv ) );

    // Only first 8 bytes are allowed to contain data.
    KMAP_ENSURE( tv == 0, error_code::common::conversion_failed );

    std::copy( std::begin( id.data ), std::begin( id.data ) + sizeof( uint64_t )
             , reinterpret_cast< char* >( &tv ) );


    rv = tv;

    return rv;
}

// TODO: 'id' Should probably be a integral constant (C++20)
auto to_uuid( uint64_t const& id )
    -> Uuid
{
    auto rv = Uuid{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            //BC_ASSERT( to_uint64( rv ) == id ); TODO?
        })
    ;

    auto const* id_bs = reinterpret_cast< char const* >( &id );
    auto const id_begin = id_bs;
    auto const id_end = id_bs + sizeof( id );

    std::copy( id_begin, id_end
             , std::begin( rv.data ) );

    return rv;
}

auto to_uuid( std::string const& s )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    std::stringstream ss{ s };
    auto id = Uuid{};

    ss >> id;

    if( !ss.fail() )
    {
        rv = id;
    }
    else
    {
        rv = KMAP_MAKE_ERROR_MSG( error_code::common::conversion_failed, fmt::format( "cannot convert '{}' to UUID", s ) );
    }

    return rv;
}

auto longest_common_prefix( StringVec const& ss )
    -> std::string
{
    if( ss.empty() )
    {
        return {};
    }

    auto const first = ss[ 0 ];
    auto count = uint32_t{};

    for( auto const& i : views::indices( first.size() ) )
    {
        auto common = true;

        for( auto const& e : ss )
        {
            if( i >= e.size()
             || first[ i ] != e[ i ] )
            {
                common = false;
            }
        }
        
        if( common )
        {
            count = i + 1;
        }
        else
        {
            break;
        }
    }

    return first
         | views::take( count )
         | to< std::string >();
}

auto map_match_lengths( Heading const& from
                      , std::vector< Heading > const& to )
    -> std::vector< std::pair< uint32_t
                             , Heading > >
{
    auto map_len = [ from ]( std::string const& e )
        -> std::pair< uint32_t
                    , std::string >
    {
        return { match_length( from
                             , e )
               , e };
    };

    return to
         | views::transform( map_len )
         | to_vector;
}

auto match_closest( Heading const& unknown
                  , std::vector< Heading > const& knowns )
    -> Heading
{
    auto const matches = map_match_lengths( unknown
                                          , knowns )
                       | actions::sort( []( auto const& lhs
                                          , auto const& rhs ) { return lhs.first > rhs.first; } );
    auto const out = [ & ]
    {
        auto const& ult = matches[ 0 ];
        auto const& penult = matches[ 1 ];

        if( ult.first == 0) // No match length.
        {
        }
        if( ult.first == penult.first ) // Competing match length.
        {
            // TODO: what is the point of calling match_length here? Just use
            // ult.first or penult.first.
            auto const shared = match_length( ult.second
                                            , penult.second );
            auto const rv = ult.second | views::take( shared )
                                       | to< std::string >();

            return rv;
        }
        else if( unknown.size() == ult.second
                                      .size() )
        {
            return unknown;
        }
        else
        {
            return ult.second;
        }
    }();

    return out;
}

auto fetch_completions( std::string const& unknown
                      , StringVec const& knowns )
    -> StringVec
{
    auto const usize = distance( unknown );
    auto filter = views::filter( [ & ]( auto const& e )
    {
        auto const sub = e
                       | views::take( usize )
                       | to< std::string >();
        return unknown == sub;
    } );

    return knowns
         | filter
         | to< StringVec >();
}

auto markdown_to_html( std::string const& text )
    -> std::string 
{
    using emscripten::val;

    if( text.empty() )
    {
        return {};
    }

    auto v = val::global().call< val >( "convert_markdown_to_html"
                                      , text );

    if( !v.as< bool >() )
    {
        return "Error: markdown to html conversion failed";
    }
    else
    {
        return v.as< std::string >();
    }
}

auto xor_ids( Uuid const& lhs
            , Uuid const& rhs )
    -> Uuid
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( lhs.size() == 16 );
            BC_ASSERT( rhs.size() == 16 );
        })
    ;

    using std::vector;

    auto v1 = vector< uint8_t >{};
    auto v2 = vector< uint8_t >{};
    auto vc = vector< uint8_t >{};
    
    v1.resize( lhs.size() );
    v2.resize( rhs.size() );
    vc.resize( lhs.size() );

    copy( lhs.begin()
        , lhs.end()
        , v1.begin() );
    copy( rhs.begin()
        , rhs.end()
        , v2.begin() );

    for( auto const i : views::indices( lhs.size() ) )
    {
        vc[ i ] = v1[ i ] ^ v2[ i ];
    }

    auto rv = Uuid{};

    copy( vc.begin()
        , vc.end()
        , rv.begin() );

    return rv;
}

auto make_edge_id( Uuid const& from
                 , Uuid const& to )
    -> Uuid
{
    return xor_ids( from
                  , to );
}

auto url_to_heading( std::string const url )
    -> Heading
{
    return url 
         | views::replace( '.'
                         , '_' )
         | to< Heading >();
}

// TODO: make template< typename TimeUnit > rather than always chrono::seconds.
auto present_time()
    -> uint64_t
{
    using namespace std::chrono;

    return time_point_cast< seconds >( system_clock::now() )
         . time_since_epoch()
         . count();
}

auto fetch_latest_state_path()
    -> Optional< FsPath >
{
    auto to_paths = views::transform( []( auto const& e ){ return e.path(); } );
    auto filter_extension = views::filter( []( auto const& e ){ return e.extension() == ".kmap"; } );
    auto sort_by_timestamp = actions::sort( []( auto const& lhs, auto const& rhs ){ return fs::last_write_time( lhs ) > fs::last_write_time( rhs ); } );
    auto const di = fs::directory_iterator{ com::kmap_root_dir };
    auto rv = di
            | to_paths
            | filter_extension
            | to_vector
            | sort_by_timestamp;
    
    if( !rv.empty() )
    {
        return { rv.front() };
    }
    else
    {
        return nullopt;
    }
}

// This function exists purely as a workaround for the fact that lstat does not work properly on MSYS when it is called on a non-existent file.
auto file_exists( FsPath const& p )
    -> bool
{
#ifdef KMAP_MSYS
    auto ec = boost::system::error_code{};

    if( fs::exists( p
                  , ec ) )
    {
        return true;
    }
    else if( ec )
    {
        fmt::print( stderr
                  , "[Warning] `fs::exists()` failed. Note that lstat does not seem to work properly on MSYS: {}\n"
                  , ec.message() );
    }

    return false;
#else  // Not MSYS
    return fs::exists( p );
#endif // KMAP_MSYS
}

// WARNING,TODO: The current impl. does not properly merge all node attributes e.g., node bodies and aliases.
template< typename Stmts >
auto merge_trees_internal( Stmts& stmts 
                         , Uuid const& src
                         , Uuid const& dst )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( stmts.exists( src ) );
            BC_ASSERT( stmts.exists( dst ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !stmts.exists( src ) );
        })
    ;

    auto const src_children = stmts.fetch_children( src );
    auto const dst_children = stmts.fetch_children( dst );
    auto const src_map = src_children
                       | views::transform( [ &stmts ]( auto const& e ){ return std::pair{ e, stmts.fetch_heading( e ).value() }; } )
                       | to_vector;
    auto const dst_map = dst_children
                       | views::transform( [ &stmts ]( auto const& e ){ return std::pair{ e, stmts.fetch_heading( e ).value() }; } )
                       | to_vector;

    for( auto const src_child : src_map )
    {
        if( auto const it = find_if( dst_map
                                   , [ & ]( auto const& e ){ return src_child.second == e.second; } )
          ; it != end( dst_map ) )
        {
            // TODO: Merge aliases and bodies (or at least warn or fail if they are encountered).
            merge_trees_internal( stmts 
                                , src_child.first
                                , it->first );
        }
        else
        {
            stmts.move_node( src_child.first, dst );
        }
    }

    stmts.erase_node( src );
}

auto merge_trees( com::Network& nw
                , Uuid const& src 
                , Uuid const& dst )
    -> void
{
    merge_trees_internal( nw, src, dst );
}

auto merge_trees( StatementPreparer& stmts
                , Uuid const& src 
                , Uuid const& dst )
    -> void
{
    merge_trees_internal( stmts, src, dst );
}

auto merge_ranges( std::set< uint64_t > const& values )
    -> std::set< std::pair< uint64_t, uint64_t > >
{
    auto rv = std::set< std::pair< uint64_t, uint64_t > >{};
    
    if( values.empty() )
    {
        return rv;
    }

    auto first = uint64_t{ *values.begin() };
    auto second = first;

    for( uint64_t const e : values | views::drop( 1 ) )
    {
        if( ( second + 1 ) != e )
        {
            io::print( "[{:#x},{:#x}], {:#x}\n", first, second, e );
            rv.emplace( std::pair{ first, second } );

            first = e;
        }

        second = e;
    }

    rv.emplace( std::pair{ first, second } );

    return rv;
}

auto print_stacktrace()
    -> void
{
    EM_ASM(
    {
        console.log( stackTrace() );
    } );
}

auto print_tree( Kmap const& km
               , Uuid const& root
               , std::string const& indent )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );

    auto const nw = KTRY( km.fetch_component< com::Network >() );

    if( nw->is_alias( root ) )
    {
        auto const abs_path = KTRY( nw->fetch_heading( root ) );
        auto const alias_abs_path = KTRY( absolute_path_flat( km, nw->resolve( root ) ) );

        fmt::print( "{}{}[{}]\n", indent, abs_path, alias_abs_path );
    }
    else
    {
        fmt::print( "{}{}\n", indent, KTRY( nw->fetch_heading( root ) ) );
    }

    auto const children = [ & ]
    {
        if( anchor::node( root )
          | view2::left_lineal( "$" )
          | act2::exists( km ) )
        {
            return anchor::node( root )
                 | view2::child
                 | act2::to_node_set( km )
                 | ranges::to< std::vector >();
        }
        else
        {
            return anchor::node( root )
                 | view2::child
                 | act2::to_node_set( km )
                 | act::order( km );
        }
    }();

    for( auto const& child : children )
    {
        KTRY( print_tree( km, child, indent + "-" ) );
    }

    return outcome::success();
}

auto print_tree( Kmap const& km
               , Uuid const& root )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "root", root );

    auto const nw = KTRY( km.fetch_component< com::Network >() );

    if( nw->is_top_alias( root ) )
    {
        auto const abs_path = KTRY( absolute_path_flat( km, root ) );
        auto const alias_abs_path = KTRY( absolute_path_flat( km, nw->resolve( root ) ) );

        fmt::print( ">{}[{}]\n", abs_path, alias_abs_path );
    }
    else
    {
        fmt::print( ">{}\n", KTRY( absolute_path_flat( km, root ) ) );
    }

    auto const children = [ & ]
    {
        if( anchor::node( root )
          | view2::left_lineal( "$" )
          | act2::exists( km ) )
        {
            return anchor::node( root )
                 | view2::child
                 | act2::to_node_set( km )
                 | ranges::to< std::vector >();
        }
        else
        {
            return anchor::node( root )
                 | view2::child
                 | act2::to_node_set( km )
                 | act::order( km );
        }
    }();

    for(  auto const& child : children )
    {
        KTRY( print_tree( km, child, "-" ) );
    }

    return outcome::success();
}

auto copy_body( com::Network& nw 
              , Uuid const& src
              , Uuid const& dst )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "src", src );
        KM_RESULT_PUSH_NODE( "dst", dst );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( nw.fetch_body( src ) );
        })
        BC_POST([ &
                , prev_src_body = nw.fetch_body( src ) ]
        {
            if( rv )
            {
                auto const src_body = nw.fetch_body( src );
                auto const dst_body = nw.fetch_body( dst );

                BC_ASSERT( src_body
                        && dst_body
                        && src_body.value() == src_body.value() );
                BC_ASSERT( dst_body
                        && prev_src_body
                        && dst_body.value() == prev_src_body.value() );
            }
        })
    ;

    auto const src_body = KTRY( nw.fetch_body( src ) );

    KTRY( nw.update_body( dst, src_body ) );
        
    rv = outcome::success();

    return rv;
}

auto move_body( com::Network& nw
              , Uuid const& src
              , Uuid const& dst )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "src", src );
        KM_RESULT_PUSH_NODE( "dst", dst );

    auto rv = KMAP_MAKE_RESULT( void );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( nw.fetch_body( src ) );
        })
        BC_POST([ &
                , prev_src_body = nw.fetch_body( src ) ]
        {
            if( rv )
            {
                auto const src_body = nw.fetch_body( src );
                auto const dst_body = nw.fetch_body( dst );

                BC_ASSERT( src_body && src_body.value().empty() );
                BC_ASSERT( dst_body
                        && prev_src_body
                        && dst_body.value() == prev_src_body.value() );
            }
        })
    ;

    KTRY( copy_body( nw, src, dst ) );
    KTRY( nw.update_body( src, "" ) ); // TODO: Erase body. Unit test that body has been erased in DB.
        
    rv = outcome::success();

    return rv;
}

auto select_median_range( std::vector< Uuid > const& range
                        , Uuid const& median
                        , uint32_t const& max_radius  )
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( find( range, median ) != range.end() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( rv.size() <= ( max_radius * 2 ) );
        })
    ;

    auto med_it = find( range, median );

    BC_ASSERT( med_it != range.end() );

    auto const bit = [ & ]
    {
        if( std::distance( range.begin(), med_it ) < max_radius )
        {
            return range.begin();
        }
        else
        {
            return std::prev( med_it, max_radius );
        }
    }();
    auto const eit = [ & ]
    {
        if( std::distance( med_it, range.end() ) < max_radius )
        {
            return range.end();
        }
        else
        {
            return std::next( med_it, max_radius );
        }
    }();

    rv = std::vector< Uuid >{ bit, eit };

    return rv;
}

auto select_median_range( std::vector< Uuid > const& range
                        , uint32_t const& max_radius  )
    -> UuidVec
{
    if( range.empty() )
    {
        return {};
    }

    return select_median_range( range
                              , range[ range.size() / 2 ]
                              , max_radius );
}

auto fetch_siblings( Kmap const& kmap
                   , Uuid const& id )
    -> UuidSet
{
    KM_RESULT_PROLOG();

    auto rv = UuidSet{};
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( auto parent = nw->fetch_parent( id )
      ; parent )
    {
        auto const siblings = [ & ]
        {
            auto const id_set = UuidSet{ id };
            auto const children = nw->fetch_children( parent.value() );
            auto sibs = UuidSet{};

            std::set_difference( children.begin(), children.end()
                               , id_set.begin(), id_set.end()
                               , std::inserter( sibs, sibs.end() ) );

            return sibs;
        }();
        
        rv = siblings;
    }

    return rv;
}

auto fetch_siblings_ordered( Kmap const& km
                           , Uuid const& id )
    -> UuidVec
{
    KM_RESULT_PROLOG();

    auto rv = UuidVec{};
    auto const nw = KTRYE( km.fetch_component< com::Network >() );

    if( auto parent = nw->fetch_parent( id )
      ; parent )
    {
        auto const siblings = [ & ]
        {
            auto const children = anchor::node( parent.value() )
                                | view2::child
                                | act2::to_node_set( km )
                                | act::order( km );

            return children
                 | views::remove( id )
                 | to_vector;
        }();
        
        rv = siblings;
    }

    return rv;
}

auto fetch_siblings_inclusive( Kmap const& kmap
                             , Uuid const& id )
    -> UuidSet
{
    return anchor::node( id )
         | view2::parent
         | view2::child
         | act2::to_node_set( kmap );
}

auto fetch_siblings_inclusive_ordered( Kmap const& kmap
                                     , Uuid const& id )
    -> UuidVec
{
    return fetch_siblings_inclusive( kmap, id )
         | act::order( kmap );
}

auto to_heading_path( Kmap const& kmap
                    , UuidVec const& lineage )
    -> StringVec
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    return lineage
         | views::transform( [ & ]( auto const& e ){ return KTRYE( nw->fetch_heading( e ) ); } )
         | to< StringVec >();
}

auto to_heading_path_flat( Kmap const& kmap
                         , UuidVec const& lineage )
    -> Heading
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    auto rv = lineage 
            | views::transform( [ & ]( auto const& e ){ return KTRYE( nw->fetch_heading( e ) ); } )
            | to_vector;

    return rv 
         | views::join( '.' )
         | to< Heading >();
}

auto flatten( StringVec const& v
            , char const c )
    -> std::string
{
    return v
         | views::join( c )
         | to< std::string >();
}

// TODO: This isn't fully correct. It's correct when the nodes are siblings, but order isn't taken into account they're not.
//       Assuming order means top to bottom of the network.
auto is_ordered( Kmap const& kmap
               , Uuid const& former 
               , Uuid const& latter )
    -> bool
{
    KM_RESULT_PROLOG();

    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    if( nw->is_sibling( former, latter ) )
    {
        return KTRYE( nw->fetch_ordering_position( former ) ) < KTRYE( nw->fetch_ordering_position( latter ) );
    }
    else
    {
        return nw->distance( kmap.root_node_id(), former ) < nw->distance( kmap.root_node_id(), latter );
    }
}

} // namespace kmap
