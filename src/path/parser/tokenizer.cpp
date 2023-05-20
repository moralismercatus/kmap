/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/parser/tokenizer.hpp>

#include <common.hpp>
#include <test/util.hpp>
#include <util/result.hpp>
#include <utility.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/zip.hpp>

#include <any>
#include <iostream>
#include <variant>

namespace rvs = ranges::views;

namespace kmap::ast::path {

using boost::fusion::operator<<;

auto flatten( ast::path::HeadingPath const& hp )
    -> std::vector< ast::path::HeadingPath >
{
    auto rv = std::vector< ast::path::HeadingPath >{};

    boost::apply_visitor( [ & ]( auto const& arg ) mutable
    {
        using T = std::decay_t< decltype( arg ) >;

        auto targ = arg;
        targ.child = boost::none;

        rv.emplace_back( targ );

        if constexpr( std::is_same_v< T, BwdDelim > )
        {
            if( arg.child )
            {
                auto const descs = boost::apply_visitor( [ & ]( auto const& child ) mutable
                {
                    using U = std::decay_t< decltype( child ) >;

                    if constexpr( std::is_same_v< U, BwdDelim > )
                    {
                        return flatten( HeadingPath{ child } );
                    }
                    else if constexpr( std::is_same_v< U, x3::forward_ast< DisAmbigDelim > >
                                    || std::is_same_v< U, x3::forward_ast< FwdDelim > >
                                    || std::is_same_v< U, x3::forward_ast< Heading > >
                                    || std::is_same_v< U, x3::forward_ast< Tag > > )
                    {
                        return flatten( HeadingPath{ child.get() } );
                    }
                    else
                    {
                        static_assert( always_false< U >::value, "non-exhaustive visitor!" );
                    }
                }
                , arg.child.value().get() );

                rv.insert( rv.end(), descs.begin(), descs.end() );
            }
        }
        else if constexpr( std::is_same_v< T, DisAmbigDelim > )
        {
            if( arg.child )
            {
                auto const descs = boost::apply_visitor( [ & ]( auto const& child ) mutable
                {
                    using U = std::decay_t< decltype( child ) >;

                    if constexpr( std::is_same_v< U, x3::forward_ast< Heading > >
                               || std::is_same_v< U, x3::forward_ast< Tag > > )
                    {
                        return flatten( HeadingPath{ child.get() } );
                    }
                    else
                    {
                        static_assert( always_false< U >::value, "non-exhaustive visitor!" );
                    }
                }
                , arg.child.value().get() );

                rv.insert( rv.end(), descs.begin(), descs.end() );
            }
        }
        else if constexpr( std::is_same_v< T, FwdDelim > )
        {
            if( arg.child )
            {
                auto const descs = boost::apply_visitor( [ & ]( auto const& child ) mutable
                {
                    using U = std::decay_t< decltype( child ) >;

                    if constexpr( std::is_same_v< U, FwdDelim > )
                    {
                        return flatten( HeadingPath{ child } );
                    }
                    else if constexpr( std::is_same_v< U, x3::forward_ast< BwdDelim > >
                                    || std::is_same_v< U, x3::forward_ast< DisAmbigDelim > >
                                    || std::is_same_v< U, x3::forward_ast< Heading > >
                                    || std::is_same_v< U, x3::forward_ast< Tag > > )
                    {
                        return flatten( HeadingPath{ child.get() } );
                    }
                    else
                    {
                        static_assert( always_false< U >::value, "non-exhaustive visitor!" );
                    }
                }
                , arg.child.value().get() );

                rv.insert( rv.end(), descs.begin(), descs.end() );
            }
        }
        else if constexpr( std::is_same_v< T, Heading > )
        {
            if( arg.child )
            {
                auto const descs = boost::apply_visitor( [ & ]( auto const& child ) mutable
                {
                    using U = std::decay_t< decltype( child ) >;

                    if constexpr( std::is_same_v< U, x3::forward_ast< BwdDelim > >
                               || std::is_same_v< U, x3::forward_ast< DisAmbigDelim > >
                               || std::is_same_v< U, x3::forward_ast< FwdDelim > >
                               || std::is_same_v< U, x3::forward_ast< Tag > > )
                    {
                        return flatten( HeadingPath{ child.get() } );
                    }
                    else
                    {
                        static_assert( always_false< U >::value, "non-exhaustive visitor!" );
                    }
                }
                , arg.child.value().get() );

                rv.insert( rv.end(), descs.begin(), descs.end() );
            }
        }
        else if constexpr( std::is_same_v< T, Tag > )
        {
            if( arg.child )
            {
                auto const descs = boost::apply_visitor( [ & ]( auto const& child ) mutable
                {
                    using U = std::decay_t< decltype( child ) >;

                    if constexpr( std::is_same_v< U, Tag > )
                    {
                        return flatten( HeadingPath{ child } );
                    }
                    else if constexpr( std::is_same_v< U, x3::forward_ast< BwdDelim > >
                                    || std::is_same_v< U, x3::forward_ast< DisAmbigDelim > >
                                    || std::is_same_v< U, x3::forward_ast< FwdDelim > > )
                    {
                        return flatten( HeadingPath{ child.get() } );
                    }
                    else
                    {
                        static_assert( always_false< U >::value, "non-exhaustive visitor!" );
                    }
                }
                , arg.child.value().get() );

                rv.insert( rv.end(), descs.begin(), descs.end() );
            }
        }
        else
        {
            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
        }
    }
    , hp );

    return rv;
}

auto to_string( ast::path::HeadingPath const& hp )
    -> std::string
{
    auto rv = std::string{};

    auto const v = to_string_vec( hp );

    rv = v
       | rvs::join
       | ranges::to< std::string >();

    return rv;
}

auto to_string_vec( ast::path::HeadingPath const& hp )
    -> std::vector< std::string >
{
    auto rv = std::vector< std::string >{};

    for( auto const flattened = flatten( hp )
       ; auto const& elem : flattened )
    {
        boost::apply_visitor( [ & ]( auto const& arg )
        {
            using T = std::decay_t< decltype( arg ) >;

            if constexpr( std::is_same_v< T, Tag > )
            {
                rv.emplace_back( '#' + std::string{ arg.value } );
            }
            else
            {
                rv.emplace_back( std::string{ arg.value } );
            }
        }
        , elem );
    }

    return rv;
}

} // kmap::ast::path

// Note: for whatever reason, these must be declared in global namespace.
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::Heading, value, child )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::Tag, value, child )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::BwdDelim, value, child )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::FwdDelim, value, child )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::path::DisAmbigDelim, value, child )

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
    struct fwd_class;
    struct heading_class;
    struct heading_path_class;
    struct tag_class;

    x3::rule< bwd_class, ast::path::BwdDelim > const bwd = "bwd";
    x3::rule< disambig_class, ast::path::DisAmbigDelim > const dis = "dis";
    x3::rule< fwd_class, ast::path::FwdDelim > const fwd = "fwd";
    x3::rule< heading_class, ast::path::Heading > const heading = "heading";
    x3::rule< heading_path_class, ast::path::HeadingPath > const heading_path = "heading_path";
    x3::rule< tag_class, ast::path::Tag > const tag = "tag";

    auto const heading_char
        = alnum | char_( '_' ) 
        ;
    auto const heading_def
        = +heading_char
       >> -( bwd | dis | fwd | tag )
        ;
    auto const tag_def
        = lit( '#' )
       >> *heading_char
       >> -( bwd | dis | fwd | tag )
        ;
    auto const bwd_def
        = char_( ',' )
       >> -( bwd | dis | fwd | tag | heading )
        ;
    auto const fwd_def
        = char_( '.' )
       >> -( bwd | dis | fwd | tag | heading )
        ;
    auto const dis_def
        = char_( '\'' )
       >> -( tag | heading )
        ;
    auto const heading_path_def
        = expect[ bwd | dis | fwd | tag | heading ]
        ;

    BOOST_SPIRIT_DEFINE
    (
        bwd
    ,   dis
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
        using boost::spirit::x3::error_handler_tag;
        using iterator_type = std::string_view::const_iterator;
        using error_handler_type = boost::spirit::x3::error_handler< iterator_type >;

        KM_RESULT_PROLOG();

        KMAP_ENSURE( !raw.empty(), error_code::common::uncategorized );
        KMAP_ENSURE( raw.find( ' ' ) == std::string::npos, error_code::common::uncategorized );

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

        auto const check = [ & ]( std::string const& input )
        {
            auto const ts = KTRYB( tokenize_heading_path( input ) );
            return to_string( ts ) == input;
        };

        // Empty path
        REQUIRE( !check( "" ) );
        // Space
        REQUIRE( !check( " " ) );
        // ,
        {
            auto const prefix = std::string{ "," };
            REQUIRE( check( prefix + "" ) );
            REQUIRE( check( prefix + "," ) );
            REQUIRE( check( prefix + "'" ) );
            REQUIRE( check( prefix + "." ) );
            REQUIRE( check( prefix + "test_heading" ) );
            REQUIRE( check( prefix + "#" ) );
            REQUIRE( check( prefix + "#test_tag" ) );
        }
        // '
        {
            auto const prefix = std::string{ "'" };
            REQUIRE( check( prefix + "" ) );
            REQUIRE( !check( prefix + "," ) );
            REQUIRE( !check( prefix + "'" ) );
            REQUIRE( !check( prefix + "." ) );
            REQUIRE( check( prefix + "test_heading" ) );
            REQUIRE( check( prefix + "#" ) );
            REQUIRE( check( prefix + "#test_tag" ) );
        }
        // .
        {
            auto const prefix = std::string{ "." };
            REQUIRE( check( prefix + "" ) );
            REQUIRE( check( prefix + "," ) );
            REQUIRE( check( prefix + "'" ) );
            REQUIRE( check( prefix + "." ) );
            REQUIRE( check( prefix + "test_heading" ) );
            REQUIRE( check( prefix + "#" ) );
            REQUIRE( check( prefix + "#test_tag" ) );
        }
        // #
        {
            auto const prefix = std::string{ "#" };
            REQUIRE( check( prefix + "" ) );
            REQUIRE( check( prefix + "," ) );
            REQUIRE( check( prefix + "'" ) );
            REQUIRE( check( prefix + "." ) );
            REQUIRE( check( prefix + "test_heading" ) );
            REQUIRE( check( prefix + "#" ) );
            REQUIRE( check( prefix + "#test_tag" ) );
        }
        // <heading>
        {
            auto const prefix = std::string{ "test_heading" };
            REQUIRE( check( prefix + "" ) );
            REQUIRE( check( prefix + "," ) );
            REQUIRE( check( prefix + "'" ) );
            REQUIRE( check( prefix + "." ) );
            REQUIRE( check( prefix + "#" ) );
            REQUIRE( check( prefix + "#test_tag" ) );
        }
    }

} // namespace kmap::parser::path