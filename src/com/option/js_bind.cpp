/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/option/option.hpp>

#include <emscripten/bind.h>

namespace kmap::com {

namespace binding {

using namespace emscripten;

struct OptionStore
{
    Kmap& km;

    OptionStore( Kmap& kmap )
        : km{ kmap }
    {
    }

    auto apply( Uuid const& option )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_STR( "option", option );

        auto const ostore = KTRY( km.fetch_component< com::OptionStore >() );

        return ostore->apply( option );
    }

    auto apply_all()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const ostore = KTRY( km.fetch_component< com::OptionStore >() );

        return ostore->apply_all();
    }

    auto update_value( std::string const& path
                     , float const& value )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_STR( "path", path );
            KM_RESULT_PUSH_STR( "value", std::to_string( value ) );

        auto const ostore = KTRY( km.fetch_component< com::OptionStore >() );

        return ostore->update_value( path, fmt::format( "{:.2f}", value ) );
    }
};

auto option_store()
    -> com::binding::OptionStore
{
    return com::binding::OptionStore{ kmap::Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_option_store )
{
    function( "option_store", &kmap::com::binding::option_store );
    class_< kmap::com::binding::OptionStore >( "OptionStore" )
        .function( "apply", &kmap::com::binding::OptionStore::apply )
        .function( "apply_all", &kmap::com::binding::OptionStore::apply_all )
        .function( "update_value", &kmap::com::binding::OptionStore::update_value )
        ;
}

} // namespace binding
} // namespace kmap::com


