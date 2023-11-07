/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/cli/cli.hpp>
#include <kmap.hpp>

#include <emscripten/bind.h>

namespace kmap::com {
namespace binding {

using namespace emscripten;

struct Cli
{
    kmap::Kmap& km;

    auto clear_input()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG(); 

        auto const cli = KTRY( km.fetch_component< com::Cli >() );

        return cli->clear_input();
    }
    auto enable_write()
        -> void
    {
        KM_RESULT_PROLOG(); 

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->enable_write();
    }
    auto focus()
        -> void
    {
        KM_RESULT_PROLOG(); 

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->focus();
    }
    auto on_key_down( int const key
                    , bool const is_ctrl
                    , bool const is_shift
                    , std::string const& text )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRY( km.fetch_component< com::Cli >() );

        return cli->on_key_down( key
                               , is_ctrl 
                               , is_shift
                               , text );
    }
    auto notify_success( std::string const& message )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        KTRYE( cli->notify_success( message ) );
    }
    auto notify_failure( std::string const& message )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        KTRYE( cli->notify_failure( message ) );
    }
    auto parse_cli( std::string const& input )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->parse_cli( input );
    }
    auto write( std::string const& input )
        -> void
    {
        KM_RESULT_PROLOG();

        auto const cli = KTRYE( km.fetch_component< com::Cli >() );

        cli->write( input );
    }
};

auto cli()
    -> com::binding::Cli
{
    return com::binding::Cli{ kmap::Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_cli )
{
    function( "cli", &kmap::com::binding::cli );
    class_< kmap::com::binding::Cli >( "Cli" )
        .function( "clear_input", &kmap::com::binding::Cli::clear_input )
        .function( "enable_write", &kmap::com::binding::Cli::enable_write )
        .function( "focus", &kmap::com::binding::Cli::focus )
        .function( "notify_failure", &kmap::com::binding::Cli::notify_failure )
        .function( "notify_success", &kmap::com::binding::Cli::notify_success )
        .function( "on_key_down", &kmap::com::binding::Cli::on_key_down )
        .function( "parse_cli", &kmap::com::binding::Cli::parse_cli )
        .function( "write", &kmap::com::binding::Cli::write );
        ;
}

} // namespace binding
} // namespace kmap::com
