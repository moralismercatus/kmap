/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../../cli/parser.hpp"

#include "../../common.hpp"
#include "../../io.hpp"
#include "../master.hpp"

#include <boost/test/unit_test.hpp>

#include <string_view>

namespace utf = boost::unit_test;

namespace kmap::test {

namespace {

auto check_solocommand( ast::cli::CommandBar const& cbar
                      , std::string_view const cmd_str )
    -> void
{
    boost::apply_visitor( [ & ]( auto const& e )
    {
        using T = std::decay_t< decltype( e ) >;

        if constexpr( std::is_same_v< T, kmap::ast::cli::Selection > )
        {
            BOOST_TEST( false );
        }
        else if constexpr( std::is_same_v< T, kmap::ast::cli::Command > )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, kmap::ast::cli::SoloCommand > )
                {
                    BOOST_TEST( e.command == cmd_str );
                }
                else if constexpr( std::is_same_v< T, kmap::ast::cli::CommandArgPair > )
                {
                    BOOST_TEST( false );
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
}

auto check_command_arg_pair( ast::cli::CommandBar const& cbar
                           , std::string_view const cmd_str
                           , std::string_view const arg_str )
    -> void
{
    boost::apply_visitor( [ & ]( auto const& e )
    {
        using T = std::decay_t< decltype( e ) >;

        if constexpr( std::is_same_v< T, kmap::ast::cli::Selection > )
        {
            BOOST_TEST( false );
        }
        else if constexpr( std::is_same_v< T, kmap::ast::cli::Command > )
        {
            boost::apply_visitor( [ & ]( auto const& e )
            {
                using T = std::decay_t< decltype( e ) >;

                if constexpr( std::is_same_v< T, kmap::ast::cli::SoloCommand > )
                {
                    BOOST_TEST( false );
                }
                else if constexpr( std::is_same_v< T, kmap::ast::cli::CommandArgPair > )
                {
                    BOOST_TEST( e.command == cmd_str );
                    BOOST_TEST( e.arg == arg_str );
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
}

auto check_command( ast::cli::Command const& cmd
                  , Optional< std::string_view > const cmd_str
                  , Optional< std::string_view > const arg_str )
    -> void
{
    boost::apply_visitor( [ & ]( auto const& e )
    {
        using T = std::decay_t< decltype( e ) >;

        if constexpr( std::is_same_v< T, kmap::ast::cli::SoloCommand > )
        {
            BOOST_REQUIRE( cmd_str.has_value() );
            BOOST_TEST( e.command == *cmd_str );
        }
        else if constexpr( std::is_same_v< T, kmap::ast::cli::CommandArgPair > )
        {
            BOOST_REQUIRE( cmd_str.has_value() );
            BOOST_TEST( e.command == *cmd_str );
            BOOST_REQUIRE( arg_str.has_value() );
            BOOST_TEST( e.arg == *arg_str );
        }
        else
        {
            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
        }
    }
    , cmd );
}

auto check_soloselection( ast::cli::CommandBar const& cbar
                        , std::string_view const sel_str )
    -> void
{
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
                    BOOST_TEST( e.selection.heading == sel_str );
                }
                else if constexpr( std::is_same_v< T, kmap::ast::cli::SelectionCommandPair > )
                {
                    BOOST_TEST( false );
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
            BOOST_TEST( false );
        }
        else
        {
            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
        }
    }
    , cbar );
}

auto check_selection_command_pair( ast::cli::CommandBar const& cbar
                                 , std::string_view const sel_str
                                 , Optional< std::string_view > const cmd_str
                                 , Optional< std::string_view > const arg_str )
    -> void
{
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
                    BOOST_TEST( false );
                }
                else if constexpr( std::is_same_v< T, kmap::ast::cli::SelectionCommandPair > )
                {
                    BOOST_TEST( e.selection.heading == sel_str );
                    check_command( e.command, cmd_str, arg_str );
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
            BOOST_TEST( false );
        }
        else
        {
            static_assert( always_false< T >::value, "non-exhaustive visitor!" );
        }
    }
    , cbar );
}

} // namespace anon

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( cli )
/******************************************************************************/
BOOST_AUTO_TEST_SUITE( parser )
/******************************************************************************/
BOOST_AUTO_TEST_CASE( soloselection )
{
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":@1" )
              . has_value() );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( "@1.2" )
              . has_value() == false );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":@1." )
              . has_value() );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":@1.2" )
              . has_value() );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":@1.2_3.4" )
              . has_value() );

    {
        auto const sel_str = "1.2_3";
        auto const cbar = kmap::parser::cli::parse_command_bar( fmt::format( ":@{}"
                                                                           , sel_str ) );
        BOOST_REQUIRE( cbar.has_value() );

        check_soloselection( *cbar, sel_str );
    }
}

BOOST_AUTO_TEST_CASE( solocommand )
{
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":1" )
              . has_value() );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( "1.2" )
              . has_value() == false );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":1." )
              . has_value() );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":1.2" )
              . has_value() );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":1.2_3.4" )
              . has_value() );

    {
        auto const cmd_str = "1.2";
        auto const cbar = kmap::parser::cli::parse_command_bar( fmt::format( ":{}"
                                                                           , cmd_str ) );
        BOOST_REQUIRE( cbar.has_value() );

        check_solocommand( *cbar, cmd_str );
    }
}

BOOST_AUTO_TEST_CASE( command_arg_pair
                    ,
                    * utf::depends_on( "cli/parser/solocommand" ) )
{
    {
        auto const cmd_str = "1.2";
        auto const arg_str = "3.4 5.7.  !@#$";
        auto const cbar = kmap::parser::cli::parse_command_bar( fmt::format( ":{} {}"
                                                                           , cmd_str
                                                                           , arg_str ) );
        BOOST_REQUIRE( cbar.has_value() );

        check_command_arg_pair( *cbar, cmd_str, arg_str );
    }
}

BOOST_AUTO_TEST_CASE( select_command_pair
                    ,
                    * utf::depends_on( "cli/parser/soloselection" )
                    * utf::depends_on( "cli/parser/command_arg_pair" ) )
{
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":@1.2 :3.4" )
              . has_value() );
    BOOST_TEST( kmap::parser::cli::parse_command_bar( ":@1.2 :3.4 5.6" )
              . has_value() );

    {
        auto const sel_str = "1.2";
        auto const cmd_str = "1.2";
        auto const arg_str = "3.4 5.7.  !@#$";
        auto const cbar = kmap::parser::cli::parse_command_bar( fmt::format( ":@{}  :{} {}"
                                                                           , sel_str
                                                                           , cmd_str
                                                                           , arg_str ) );
        BOOST_REQUIRE( cbar.has_value() );

        check_selection_command_pair( *cbar
                                    , sel_str
                                    , Optional< std::string_view >{ cmd_str }
                                    , Optional< std::string_view >{ arg_str } );
    }
}

BOOST_AUTO_TEST_SUITE_END( /* parser */ )
BOOST_AUTO_TEST_SUITE_END( /* cli */ )

} // namespace kmap::test