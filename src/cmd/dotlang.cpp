/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "dotlang.hpp"

#include "../common.hpp"
#include "../contract.hpp"
#include "../io.hpp"
#include "../kmap.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/optional.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/variant/static_visitor.hpp>
#include <range/v3/action/insert.hpp>
#include <range/v3/action/remove.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/group_by.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/unique.hpp>
// #include <boost/interprocess/file_mapping.hpp>
// #include <boost/interprocess/mapped_region.hpp>
//#include <boost/optional/optional_io.hpp>

#include <string>
#include <variant>
#include <sstream>
#include <unordered_map>
#include <set>
#include <iostream>
#include <stdio.h>
// #include <unistd.h>
#include <sys/types.h>
// #include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

using namespace ranges;
namespace fs = boost::filesystem;

// TODO: This should really be separated into a utility. Dotlang parsing isn't necessarily a command-specific functionality.
//       The challenge is that, from my reading, it's a little tricky to expose the interface in a header.
namespace ast::dotlang {

namespace x3 = boost::spirit::x3;

struct KvPair : x3::position_tagged
{
    std::string key;
    std::string value;
};

struct Decl : x3::position_tagged
{
    std::string id;
    std::vector< KvPair > attrs;
};

struct Link : x3::position_tagged
{
    std::string from;
    std::string to;
};

struct Dot : x3::position_tagged
{
    // TODO: Should replace LineItem with `struct LineItem : x3::variant< Link, Decl >`, as I've had extreme difficulty elsewhere getting any other variant type to cooperate with Spirit.
    using LineItem = std::variant< Link
                                 , Decl >;
    //using LineItem = Link;
    std::string graph_type;
    std::string graph_id; // TODO: Should be std/boost::optional. Can't get it to work.
    std::string graph_label; // TODO: Should be std/boost::optional. Can't get it to work.
    std::vector< LineItem > line_items;
    //std::string line_item1;
    //std::string line_item2;
    //LineItem line_items;
};

using boost::fusion::operator<<;

} // namespace ast::dotlang

BOOST_FUSION_ADAPT_STRUCT( ast::dotlang::KvPair
                         , key 
                         , value )
BOOST_FUSION_ADAPT_STRUCT( ast::dotlang::Decl
                         , id
                         , attrs )
BOOST_FUSION_ADAPT_STRUCT( ast::dotlang::Link
						 , from 
						 , to )
BOOST_FUSION_ADAPT_STRUCT( ast::dotlang::Dot
						 , graph_type 
		  				 , graph_id
		  				 , graph_label
                         , line_items )

// TODO: WARNING: This parser is not completely conforming. Improve as needed toward conformance.
// Refer to https://www.graphviz.org/doc/info/lang.html for complete spec.
namespace parser::dotlang
{
    namespace x3 = boost::spirit::x3;
	namespace ascii = boost::spirit::x3::ascii;
	using x3::lit;
    using x3::expect;
    using x3::lexeme;
	using ascii::char_;
	using ascii::alnum;

    struct ErrorHandler
    {
        template< typename Iterator
                , typename Exception
                , typename Context >
        auto on_error( Iterator& first
                     , Iterator const& last
                     , Exception const& x
                     , Context const& context )
            -> x3::error_handler_result
        {
            auto& error_handler = x3::get< x3::error_handler_tag >( context ).get();
            auto message = std::string{ "Error! Expecting: " + x.which() + " here:" };

            error_handler( x.where()
                         , message );

            return x3::error_handler_result::fail;
        }
    };

    struct kvpair_class;
    struct decl_class;
    struct link_class;
    struct dot_class;

    // TODO: Link should be KvPair.
    x3::rule< kvpair_class, ast::dotlang::Link > const kvpair = "kvpair";
    x3::rule< link_class, ast::dotlang::Link > const link = "link";
    x3::rule< decl_class, ast::dotlang::Decl > const decl = "decl";
    x3::rule< dot_class, ast::dotlang::Dot > const dot = "dot";

    auto const quoted_string
        = lexeme[ '"' >> +( ( '\\' >> char_( '"' ) ) | ( char_ - '"' ) ) >> '"' ]
        ;
    auto const arrow
        = lit( '-' )
       >> '>'
        ;
    auto const graph 
        = x3::string( "graph" ) | x3::string( "digraph" )
        ;
    auto const label
        = lit( "label" )
       >> '='
       >> quoted_string
        ;
    auto const graph_label
        = label
       >> ';'
        ;
    auto const kv_key
        = +( alnum - '=' )
        ;
    auto const kv_value
        = quoted_string | +( alnum )
        ;
    auto const kvpair_def
        = kv_key
       >> '='
       >> kv_value
        ;
    auto const node_id
        = +( alnum )
        ;
    auto const decl_def
        = node_id
       >> '['
       >> ( kvpair % ',' )
       >> ']'
       >> ';'
        ;
    auto const link_def
        = node_id
       >> arrow
       >> node_id
       >> ';'
        ;
    auto const dot_def
        = expect[ graph ]
        > quoted_string
        > '{'
        > graph_label
        > *( link | decl )
        > '}'
        ;

	BOOST_SPIRIT_DEFINE( kvpair, link, decl, dot );

    struct kvpair_class : ErrorHandler, x3::annotate_on_success {};
    struct link_class : ErrorHandler, x3::annotate_on_success {};
    struct decl_class : ErrorHandler, x3::annotate_on_success {};
    struct dot_class : ErrorHandler, x3::annotate_on_success {};

    auto parse_dot( std::string_view const dotstr )
        -> boost::optional< ast::dotlang::Dot >
    { 
        using boost::spirit::x3::with;
        using boost::spirit::x3::ascii::space;
        using boost::spirit::x3::error_handler_tag;
        using iterator_type = std::string_view::const_iterator;
        using error_handler_type = boost::spirit::x3::error_handler< iterator_type >;

        auto dot = ast::dotlang::Dot{};
        auto db = dotstr.begin();
        auto const de = dotstr.end();
        auto errstrm = std::stringstream{};
        auto error_handler = error_handler_type{ db
                                               , de
                                               , errstrm };
        auto const parser = with< error_handler_tag >( std::ref( error_handler ) )[ parser::dotlang::dot ];
        auto const r = phrase_parse( db
                                   , de
                                   , parser
                                   , space
                                   , dot );

        if (r && db == de )
        {
            return { dot };

        }
        else
        {
            // TODO: errstrm.rdbuf() should be returned to the caller somehow (Boost.Outcome?)
            std::cout << "parse failed" << std::endl;
            std::cout << errstrm.rdbuf() << std::endl;

            return {};
        }
    }

    auto parse_dot( fs::path const& fp )
        -> boost::optional< ast::dotlang::Dot >
    { 
        auto rv = boost::optional< ast::dotlang::Dot >{};

        if( auto ifs = fs::ifstream{ fp
                                   , std::ios::binary }
          ; ifs.good() )
        {
            auto buffer = std::vector< char >{};
            auto fsize = fs::file_size( fp );

            buffer.resize( fsize );

            if( ifs.read( buffer.data()
                        , fsize ) )
            {
                auto const sv = std::string_view{ buffer.data()
                                                , buffer.size() };

                rv = parse_dot( sv );
            }
        }
        else
        {
            fmt::print( "unable to open file {}\n"
                      , fp.string() );
        }

        return rv;
    }
} // namespace parser::dotlang

namespace kmap::cmd {

using Link = std::pair< Uuid
                      , Uuid >;
using Links = std::vector< Link >;
using LinkSet = std::vector< Link >;
using LinkMultimap = std::multimap< Uuid
                                  , Uuid >;

// TODO: This might belong in utility.
auto children( Uuid const& parent
             , LinkMultimap const& link_map )
    -> std::vector< Uuid >
{
    auto const eq = link_map.equal_range( parent );
    auto const kvs = Links{ eq.first
                          , eq.second };

    return kvs 
         | views::values
         | to< UuidVec >();
}

auto replace_multiparent_with_alias( Uuid const& parent
                                   , Uuid const& child
                                   , LinkMultimap& c_to_p_map
                                   , LinkMultimap& p_to_c_map
                                   , Links& aliases )
                                   -> void
{
    if( c_to_p_map.count( child ) > 1 )
    {
        auto const er = c_to_p_map.equal_range( child );
        auto const parent_range = Links{ er.first
                                       , er.second };
        for( auto const& e : parent_range )
        {
            if( e != Link{ parent, child } ) 
            {
                {
                    auto eq = c_to_p_map.equal_range( e.first );

                    for( auto it = eq.first
                       ; it != eq.second
                       ; ++it )
                    {
                        if( Link{ *it } == e )
                        {
                            c_to_p_map.erase( it );
                            break;
                        }
                    }
                }
                {
                    auto eq = p_to_c_map.equal_range( e.second );

                    for( auto it = eq.first
                       ; it != eq.second
                       ; ++it )
                    {
                        if( Link{ *it } == Link{ e.second, e.first } )
                        {
                            p_to_c_map.erase( it );
                            break;
                        }
                    }
                }

                aliases.emplace_back( e );
            }
        }
    }

    for( auto const& e : children( child
                                 , p_to_c_map ) )
    {
        replace_multiparent_with_alias( child
                                      , e
                                      , c_to_p_map
                                      , p_to_c_map
                                      , aliases );
    }
}

auto process( ast::dotlang::Dot const& dot
            , Uuid const& root )
    -> Optional< StatementPreparer >
{
    auto rv = Optional< StatementPreparer >{};

    struct line_item_visitor : boost::static_visitor<>
    {
        using IdMap = std::unordered_map< std::string
                                        , Uuid >;

        line_item_visitor( StatementPreparer& prep
                         , IdMap& id_map
                         , Uuid const& root )
            : prep_{ prep }
            , id_map_{ id_map }
            , root_{ root }
        {
        }

        auto get_or_create_id( std::string const& id )
            -> Uuid
        {
            if( auto const it = id_map_.find( id )
              ; it != id_map_.end() )
            {
                return it->second;
            }
            else
            {
                return id_map_.emplace( id
                                      , gen_uuid() )
                              .first
                             ->second;
            }
        }

        auto operator()( ast::dotlang::Link const& link )
            -> void
        {
            auto const fid = get_or_create_id( link.from );
            auto const tid = get_or_create_id( link.to );

            if( !prep_.exists( fid ) )
            {
                prep_.create_child( root_
                                  , fid
                                  , link.from );
            }

            if( prep_.exists( tid )
             && *prep_.fetch_parent( tid ) == root_ )
            {
                prep_.move_node( tid
                               , fid );
            }
            else if( prep_.exists( tid )
                  && *prep_.fetch_parent( tid ) != root_ )
            {
                // The graph has more than one parent for the child. Kmap doesn't allow this,
                // so compensate by aliasing the existent one.
                auto const alias = prep_.create_alias( tid
                                                     , fid );
                
                BC_ASSERT( alias );
            }
            else
            {
                prep_.create_child( fid
                                  , tid
                                  , link.to );
            }
        }

        auto operator()( ast::dotlang::Decl const& decl )
            -> void
        {
            auto const id = get_or_create_id( decl.id );

            if( !prep_.exists( id ) )
            {
                prep_.create_child( root_
                                  , id
                                  , decl.id );
            }

            auto const label = [ & ]
            {
                for( auto const& e : decl.attrs )
                {
                    if( e.key == "label" )
                    {
                        auto lab = std::string{};

                        lab = boost::replace_all_copy( e.value
                                                     , "&nbsp;"
                                                     , " " );
                        lab = boost::replace_all_copy( lab
                                                     , "\\l"
                                                     , "\n" );

                        return fmt::format( "```\n{}\n```"
                                          , lab );
                    }
                }

                return std::string{ "" };
            }();

            prep_.update_body( id
                             , label );
        }

        StatementPreparer& prep_;
        IdMap& id_map_;
        Uuid root_;
    };

    if( dot.graph_type == "digraph" )
    {
        auto prep = StatementPreparer{};
        std::cout << "parse succeeded\n" << dot.graph_type << ":" << dot.graph_id << ":" << dot.graph_label << std::endl;
        auto id_map = line_item_visitor::IdMap{};
        auto counter = uint32_t{};
        auto const period = static_cast< uint32_t >( dot.line_items.size() * 0.05 );

        for( auto const& e : dot.line_items )
        {
            std::visit( line_item_visitor{ prep
                                         , id_map
                                         , root }
                      , e );

            if( counter % period == 0 )
            {
                auto const ts = fmt::format( "Loading DOT graph: {} percent"
                                           , ( 100 * counter ) / dot.line_items.size() );
                fmt::print( "{}\n"
                          , ts );
            }

            ++counter;
        }

        rv = prep;
    }
    
    return rv;
}

auto load_dot( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 1 );
            })
        ;
        
        auto const fp = kmap_root_dir / args[ 0 ];

        if( file_exists( fp ) )
        {
#if 0 // It appears emscripten is lacking some supported functions e.g., shmat, shmctl, etc., so using POSIX mmap in the meantime.
            using boost::interprocess::file_mapping;
            using boost::interprocess::mapped_region;
            using boost::interprocess::read_only;

            auto const fmapping = file_mapping{ fp.string().c_str()
                                              , read_only };
            auto const region = mapped_region{ fmapping
                                             , read_only };
            auto const address = static_cast< char* >( region.get_address() );
            auto const size = region.get_size();
#endif // 0
#if 0 // I can't get mmap to work either, despite emscripten having tests that seem to show it working. May be a electron isue? Must isolate later...
            if( auto const fd = open( fp.string().c_str()
                                    , O_RDONLY )
              ; fd > 0 )
            {
                //auto const fsize = fs::file_size( fp );
                auto const fsize = 10;
                auto const address = static_cast< char* >( mmap( NULL
                                                               , fsize
                                                               , PROT_READ
                                                               , MAP_PRIVATE
                                                               , fd 
                                                               , 0 ) );
                if( address != MAP_FAILED )
                {
                    std::cout << std::string_view{ address, static_cast< uint32_t >( fsize ) } << std::endl;

                     auto const rc = munmap( address
                                           , fsize );

                     assert( rc == 0 );
                }
                else
                {
                    fmt::print( "mmap error: {}\n", strerror( errno ) );
                    KMAP_MAKE_RESULT( error_code::common::uncategorized
                           , fmt::format( "unable to map file: {}"
                                        , fp.string() ) };
                }
            }
            else
            {
                KMAP_MAKE_RESULT( error_code::common::uncategorized
                       , fmt::format( "unable to open file: {}"
                                    , fp.string() ) };
            }
#endif // 0
            if( auto const dot = parser::dotlang::parse_dot( fp )
              ; dot )
            {
                fmt::print( "graph_label: {}\n", dot->graph_label );
                auto const graph_root = kmap.create_child( kmap.selected_node()
                                                         , format_heading( dot->graph_label ) );

                if( auto prep = process( *dot
                                       , graph_root.value() )
                  ; prep )
                {
                    fmt::print( "committing...\n" );
                    prep->commit( kmap );
                    fmt::print( "committed\n" );
                    KMAP_TRY( kmap.select_node( graph_root.value() ) );
                }
                else
                {
                    return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "unsupported graph type: {}", dot->graph_type ) );
                }
            }
            else
            {
                return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "parse failed; see console log" ) );
            }
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "file not found: {}", fp.string() ) );
        }

        return fmt::format( "dot file loaded" );
    };
}

} // namespace kmap::cmd