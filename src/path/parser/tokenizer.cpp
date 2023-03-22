/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/parser/tokenizer.hpp>

#include <common.hpp>
#include <test/util.hpp>
#include <util/result.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/view/zip.hpp>

#include <any>
#include <iostream>

namespace rvs = ranges::views;

namespace kmap::ast::path {

using boost::fusion::operator<<;

} // kmap::ast::path

// Note: for whatever reason, these must be declared in global namespace.
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::Heading, value )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::HeadingPath, value )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::Tag, value )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::BwdDelim, value )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::FwdDelim, value )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::DisAmbigDelim, value )

namespace kmap::parser::path
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

    struct bwd_class;
    struct disambig_class;
    struct element_class;
    struct fwd_class;
    struct heading_class;
    struct heading_path_class;
    struct tag_class;

    x3::rule< bwd_class, ast::path::BwdDelim > const bwd = "bwd";
    x3::rule< disambig_class, ast::path::DisAmbigDelim > const disambig = "disambig";
    x3::rule< element_class, ast::path::Element > const element = "element";
    x3::rule< fwd_class, ast::path::FwdDelim > const fwd = "fwd";
    x3::rule< heading_class, ast::path::Heading > const heading = "heading";
    x3::rule< heading_path_class, ast::path::HeadingPath > const heading_path = "heading_path";
    x3::rule< tag_class, ast::path::Tag > const tag = "tag";

    auto const heading_char
        = alnum | char_( '_' ) 
        ;
    auto const heading_def
        = +heading_char
        ;
    auto const tag_def
        = lit( '#' )
       >> +heading_char
        ;
    auto const bwd_def
        = char_( ',' )
        ;
    auto const fwd_def
        = char_( '.' )
        ;
    auto const disambig_def
        = char_( '\'' )
        ;
    auto const element_def
        = heading | tag | bwd | fwd | disambig
        ;
    auto const heading_path_def
        = expect[ +element ]
        ;

    BOOST_SPIRIT_DEFINE
    (
        bwd
    ,   disambig
    ,   element
    ,   fwd
    ,   heading_path
    ,   heading
    ,   tag
    );

    struct heading_class : ErrorHandler, x3::annotate_on_success {};
    struct element_class : ErrorHandler, x3::annotate_on_success {};
    struct heading_path_class : ErrorHandler, x3::annotate_on_success {};
    struct tag_class : ErrorHandler, x3::annotate_on_success {};
    struct bwd_class : ErrorHandler, x3::annotate_on_success {};
    struct fwd_class : ErrorHandler, x3::annotate_on_success {};
    struct disambig_class : ErrorHandler, x3::annotate_on_success {};

    auto tokenize_heading_path( std::string_view const raw )
        -> Result< ast::path::HeadingPath >
    {
        using boost::spirit::x3::with;
        using boost::spirit::x3::ascii::space;
        using boost::spirit::x3::no_skip;
        using boost::spirit::x3::error_handler_tag;
        using iterator_type = std::string_view::const_iterator;
        using error_handler_type = boost::spirit::x3::error_handler< iterator_type >;

        auto rv = result::make_result< ast::path::HeadingPath >();
        auto hp_ast = ast::path::HeadingPath{};
        auto rb = raw.begin();
        auto const re = raw.end();
        auto errstrm = std::stringstream{};
        auto error_handler = error_handler_type{ rb
                                               , re
                                               , errstrm };
        auto const parser = with< error_handler_tag >( std::ref( error_handler ) )[ parser::path::heading_path ];
        auto const r = parse( rb
                            , re
                            , parser
                            , hp_ast );

        if (r && rb == re )
        {
            rv = hp_ast;
        }
        else
        {
            rv = KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, errstrm.str() );
        }

        return rv;
    }

    SCENARIO( "tokenize_heading_path", "[network][parser]" )
    {
        using namespace ast::path;
        using ast::path::Heading;
        using ast::path::HeadingPath;

        auto match = [ & ]( std::string const& input, std::vector< std::any > const& expecteds ) -> Result< void >
        {
            KM_RESULT_PROLOG();

            auto const ts = KTRY( tokenize_heading_path( input ) );

            KMAP_ENSURE( ts.value.size() == expecteds.size(), error_code::common::uncategorized );

            // TODO: change to `auto const& [ token, expected ] when Clang supports relaxed lambda structured binding capture.
            for( auto const& t_and_e : rvs::zip( ts.value, expecteds ) )
            {
                auto const& token = std::get< 0 >( t_and_e );
                auto const& expected = std::get< 1 >( t_and_e );

                auto const vrv = boost::apply_visitor( [ & ]( auto const& arg )
                {
                    using T = std::decay_t< decltype( arg ) >;

                    if constexpr( std::is_same_v< T, Heading >
                               || std::is_same_v< T, Tag > )
                    {
                        return std::any_cast< T >( &expected ) != nullptr
                            && std::any_cast< T >( &expected )->value == arg.value;
                    }
                    else if constexpr( std::is_same_v< T, BwdDelim >
                                    || std::is_same_v< T, FwdDelim >
                                    || std::is_same_v< T, DisAmbigDelim > )
                    {
                        return std::any_cast< T >( &expected ) != nullptr;
                    }
                    else
                    {
                        static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                    }
                }
                , token );

                if( !vrv )
                {
                    return result::make_result< void >();
                }
            }

            return outcome::success();
        };

        GIVEN( "fundamental elements" )
        {
            THEN( "tokenized correctly" )
            {
                REQUIRE_TRY( match( "victor",  { Heading{ .value = "victor" } } ) );
                REQUIRE( test::fail( match( "victor", { Heading{ .value = "not_victor" } } ) ) );
                REQUIRE_TRY( match( "#test_tag", { Tag{ .value = "test_tag" } } ) );
                REQUIRE( test::fail( match( "#test_tag", { Tag{ .value = "not_test_tag" } } ) ) );
                REQUIRE_TRY( match( ".", { FwdDelim{} } ) );
                REQUIRE_TRY( match( ",", { BwdDelim{} } ) );
                REQUIRE_TRY( match( "'", { DisAmbigDelim{} } ) );
            }
        }
        GIVEN( "composite elements" )
        {
            THEN( "tokenized correctly" )
            {
                REQUIRE_TRY( match( "victor..charlie,charlie#echo#delta'"
                                  , { Heading{ .value = "victor" }
                                    , FwdDelim{}
                                    , FwdDelim{}
                                    , Heading{ .value = "charlie" }
                                    , BwdDelim{}
                                    , Heading{ .value = "charlie" }
                                    , Tag{ .value = "echo" }
                                    , Tag{ .value = "delta" }
                                    , DisAmbigDelim{} } ) );
            }
        }
        GIVEN( "path with space" )
        {
            THEN( "fails on encountering space" )
            {
                REQUIRE( test::fail( match( "echo .delta", { Heading{ .value = "echo" }, FwdDelim{}, Heading{ .value = "delta" } } ) ) );
            }
        }
    }

} // namespace kmap::parser::path