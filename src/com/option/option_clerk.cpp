/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/option/option_clerk.hpp"

#include "com/option/option.hpp"
#include "error/master.hpp"
#include "kmap.hpp"

#include <range/v3/view/reverse.hpp>

namespace kmap::option {

OptionClerk::OptionClerk( Kmap& km )
    : kmap{ km }
{
}

OptionClerk::~OptionClerk()
{
    try
    {
        if( auto const ostorec = kmap.fetch_component< com::OptionStore >()
          ; ostorec )
        {
            auto& ostore = ostorec.value();
            for( auto const& e : options | ranges::views::reverse )
            {
                if( auto const res = ostore->uninstall_option( e )
                ; !res && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
                {
                    KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
                }
            }
        }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto OptionClerk::apply_all()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const ostore = KTRY( kmap.fetch_component< com::OptionStore >() );

    for( auto const& opt : options )
    {
        KTRY( ostore->apply( opt ) );
    }

    rv = outcome::success();

    return rv;
}

auto OptionClerk::install_option( Heading const& heading
                                , std::string const& descr
                                , std::string const& value 
                                , std::string const& action )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const ostore = KTRY( kmap.fetch_component< com::OptionStore >() );
    auto const option = KTRY( ostore->install_option( heading, descr, value, action ) );

    options.emplace_back( option );

    rv = option;

    return rv;
}

} // namespace kmap::option
