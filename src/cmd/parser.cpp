/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "parser.hpp"

#include "error/parser.hpp"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>

#include <iostream>

namespace kmap::cmd::parser {

using boost::fusion::operator<<;

} // namespace kmap::cmd::parser

BOOST_FUSION_ADAPT_STRUCT( kmap::cmd::ast::Kscript
                         , code )
BOOST_FUSION_ADAPT_STRUCT( kmap::cmd::ast::Javascript
                         , code )

namespace kmap::cmd::parser {

namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;
using ascii::char_;
using x3::lexeme;
using x3::lit;

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

struct kscript_class;
struct javascript_class;
struct code_class;

x3::rule< kscript_class, kmap::cmd::ast::Kscript > const kscript = "kscript";
x3::rule< javascript_class, kmap::cmd::ast::Javascript > const javascript = "javascript";
x3::rule< code_class, kmap::cmd::ast::Code > const code = "code";

auto const code_delimiter
    = lit( "```" )
    ;
auto const kscript_def
    = code_delimiter >> lit( "kscript" )
   >> lexeme[ *( char_ - code_delimiter ) ]
   >> code_delimiter
    ;
auto const javascript_def
    = code_delimiter >> ( lit( "javascript" ) | lit( "js" ) )
   >> lexeme[ *( char_ - code_delimiter ) ]
   >> code_delimiter
    ;
auto const code_def
    = kscript | javascript
    ;

BOOST_SPIRIT_DEFINE( kscript
                   , javascript
                   , code )

struct kscript_class : ErrorHandler, x3::annotate_on_success {};
struct javascript_class : ErrorHandler, x3::annotate_on_success {};
struct code_class : ErrorHandler, x3::annotate_on_success {};

auto parse_body_code( std::string_view const raw )
    -> Result< cmd::ast::Code >
{
    using boost::spirit::x3::with;
    using boost::spirit::x3::ascii::space;
    using boost::spirit::x3::error_handler_tag;
    using iterator_type = std::string_view::const_iterator;
    using error_handler_type = boost::spirit::x3::error_handler< iterator_type >;

    auto rv = KMAP_MAKE_RESULT( cmd::ast::Code );
    auto cb_ast = cmd::ast::Code{};
    auto rb = raw.begin();
    auto const re = raw.end();
    auto errstrm = std::stringstream{};
    auto error_handler = error_handler_type{ rb
                                           , re
                                           , errstrm };
    auto const parser = with< error_handler_tag >( std::ref( error_handler ) )[ cmd::parser::code ];
    auto const r = phrase_parse( rb
                               , re
                               , parser
                               , space
                               , cb_ast );

    if( r && rb == re )
    {
        rv = cb_ast;
    }
    else
    {
        std::cerr << "parse failed" << std::endl;
        std::cerr << errstrm.str() << std::endl;

        rv = KMAP_MAKE_ERROR_MSG( error_code::parser::parse_failed, fmt::format( "Parse Failure: {}\nRaw: {}", errstrm.str(), raw ) );
    }

    return rv;
}

} // kmap::cmd::parser