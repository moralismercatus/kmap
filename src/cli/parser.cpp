/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/

#include "parser.hpp"

#include "../common.hpp"

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>

#include <iostream>

namespace kmap::ast::cli {

using boost::fusion::operator<<;

} // kmap::ast::cli

// Note: for whatever reason, these must be declared in global namespace.
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::cli::Heading
                         , heading )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::cli::Title
                         , title )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::cli::SoloCommand
                         , command )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::cli::CommandArgPair
                         , command
                         , arg )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::cli::SoloSelection
                         , selection )
BOOST_FUSION_ADAPT_STRUCT( kmap::ast::cli::SelectionCommandPair
                         , selection
                         , command )

namespace kmap::parser::cli
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

    struct heading_class;
    struct title_class;
    struct solocommand_class;
    struct command_arg_pair_class;
    struct command_class;
    struct soloselection_class;
    struct selection_command_pair_class;
    struct selection_class;
    struct command_bar_class;

    x3::rule< heading_class, ast::cli::Heading > const heading = "heading";
    x3::rule< title_class, ast::cli::Title > const title = "title";
    x3::rule< solocommand_class, ast::cli::SoloCommand > const solocommand = "solocommand";
    x3::rule< command_arg_pair_class, ast::cli::CommandArgPair > const command_arg_pair = "command_arg_pair";
    x3::rule< command_class, ast::cli::Command > const command = "command";
    x3::rule< soloselection_class, ast::cli::SoloSelection > const soloselection = "soloselection";
    x3::rule< selection_command_pair_class, ast::cli::SelectionCommandPair > const selection_command_pair = "selection_command_pair";
    x3::rule< selection_class, ast::cli::Selection > const selection = "selection";
    x3::rule< command_bar_class, ast::cli::CommandBar > const command_bar = "command_bar";

    auto const atsym
        = lit( '@' )
        ;
    auto const colon
        = lit( ':' )
        ;
    auto const space
        = lit( ' ' )
        ;
    auto const heading_char
        = alnum | char_( '_' ) | char_( '.' ) | char_( ',' ) | char_( '/' ) | char_( '\'' )
        ;
    auto const heading_def
        = *space
       >> +heading_char
        ;
    auto const title_def
        = lexeme[ +char_ ]
        ;
    auto const command_char
        = alnum | char_( '.' ) | char_( '_' ) // Note: differs from heading b/c of special allowed characters (e.g., '@').
        ;
    auto const command_str
        = colon
       >> +command_char
        ;
    auto const solocommand_def 
        = lexeme[ command_str
               >> !( +space >> +char_ ) ]
        ;
     auto const command_arg_pair_def
        = lexeme[ command_str
               >> +space
               >> +char_ ]
        ;
    auto const command_def
        = solocommand | command_arg_pair 
        ;
     auto const selection_str
        = colon
       >> atsym
       >> heading
        ;
    auto const soloselection_def
        = lexeme[ selection_str
               >> !( *space >> command_str ) ]
        ;
    auto const selection_command_pair_def
        = selection_str
       >> command
        ;
    auto const selection_def
        = soloselection | selection_command_pair
        ;
    auto const command_bar_def
        = expect[ selection | command ]
        ;

    BOOST_SPIRIT_DEFINE( heading
                       , title 
                       , solocommand
                       , command_arg_pair 
                       , command
                       , soloselection
                       , selection_command_pair
                       , selection
                       , command_bar );

    struct heading_class : ErrorHandler, x3::annotate_on_success {};
    struct title_class : ErrorHandler, x3::annotate_on_success {};
    struct solocommand_class : ErrorHandler, x3::annotate_on_success {};
    struct command_heading_pair_class : ErrorHandler, x3::annotate_on_success {};
    struct command_title_pair_class : ErrorHandler, x3::annotate_on_success {};
    struct command_class : ErrorHandler, x3::annotate_on_success {};
    struct soloselection_class : ErrorHandler, x3::annotate_on_success {};
    struct selection_command_class : ErrorHandler, x3::annotate_on_success {};
    struct selection_class : ErrorHandler, x3::annotate_on_success {};
    struct command_bar_class : ErrorHandler, x3::annotate_on_success {};

    auto parse_command_bar( std::string_view const raw )
        -> Optional< ast::cli::CommandBar >
    {
        using boost::spirit::x3::with;
        using boost::spirit::x3::ascii::space;
        using boost::spirit::x3::error_handler_tag;
        using iterator_type = std::string_view::const_iterator;
        using error_handler_type = boost::spirit::x3::error_handler< iterator_type >;

        auto cb_ast = ast::cli::CommandBar{};
        auto rb = raw.begin();
        auto const re = raw.end();
        auto errstrm = std::stringstream{};
        auto error_handler = error_handler_type{ rb
                                               , re
                                               , errstrm };
        auto const parser = with< error_handler_tag >( std::ref( error_handler ) )[ parser::cli::command_bar ];
        auto const r = phrase_parse( rb
                                   , re
                                   , parser
                                   , space // TODO: need to compile without space skipper. Then again, it may be easier to keep the space skipper and use lexemes when needed.
                                   , cb_ast );

        if (r && rb == re )
        {
            return { cb_ast };
        }
        else
        {
            // TODO: errstrm.rdbuf() should be returned to the caller somehow (Boost.Outcome?)
            std::cout << "parse failed" << std::endl;
            std::cout << errstrm.str() << std::endl;

            return Optional< ast::cli::CommandBar >{};
        }
    }

auto fetch_selection_string( ast::cli::CommandBar const& cbar )
    -> Optional< std::string >
{
    auto rv = Optional< std::string >{};

    boost::apply_visitor( [ & ]( auto const& e )
    {
        using T = std::decay_t< decltype( e ) >;

        if constexpr( std::is_same_v< T, kmap::ast::cli::Selection > )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, kmap::ast::cli::SoloSelection > )
                {
                    rv = e.selection.heading;
                }
                else if constexpr( std::is_same_v< T, kmap::ast::cli::SelectionCommandPair > )
                {
                    rv = e.selection.heading;
                }
                else
                {
                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                }
            }
            , e );
        }
        else if constexpr( std::is_same_v< T, kmap::ast::cli::Command > )
        {
            // Do nothing.
        }
        else
        {
            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
        }
    }
    , cbar );

    return rv;
}

auto fetch_command_string( ast::cli::CommandBar const& cbar )
    -> Optional< std::pair< std::string, std::string > >
{
    auto rv = Optional< std::pair< std::string, std::string > >{};

    boost::apply_visitor( [ & ]( auto const& e )
    {
        using T = std::decay_t< decltype( e ) >;

        if constexpr( std::is_same_v< T, kmap::ast::cli::Selection > )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, kmap::ast::cli::SoloSelection > )
                {
                    // Do nothing
                }
                else if constexpr( std::is_same_v< T, kmap::ast::cli::SelectionCommandPair > )
                {
                    boost::apply_visitor( [ & ]( auto const& e )
                    {
                        using T = std::decay_t< decltype( e ) >;

                        if constexpr( std::is_same_v< T, kmap::ast::cli::SoloCommand > )
                        {
                            rv = std::pair{ e.command, std::string{} };
                        }
                        else if constexpr( std::is_same_v< T, kmap::ast::cli::CommandArgPair > )
                        {
                            rv = std::pair{ e.command, e.arg };
                        }
                        else
                        {
                            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                        }
                    }
                    , e.command );
                }
                else
                {
                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                }
            }
            , e );
        }
        else if constexpr( std::is_same_v< T, kmap::ast::cli::Command > )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, kmap::ast::cli::SoloCommand > )
                {
                    rv = std::pair{ e.command, std::string{} };
                }
                else if constexpr( std::is_same_v< T, kmap::ast::cli::CommandArgPair > )
                {
                    rv = std::pair{ e.command, e.arg };
                }
                else
                {
                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                }
            }
            , e );
        }
        else
        {
            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
        }
    }
    , cbar );

    return rv;
}

} // namespace kmap::parser::cli