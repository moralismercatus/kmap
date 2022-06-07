/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "option/option_clerk.hpp"

#include "option/option.hpp"
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
        auto& ostore = kmap.option_store();
        for( auto const& e : options | ranges::views::reverse )
        {
            if( auto const res = ostore.uninstall_option( e )
              ; !res && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
            {
                KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
            }
        }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto OptionClerk::install_option( Heading const& heading
                                , std::string const& descr
                                , std::string const& value 
                                , std::string const& action )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& ostore = kmap.option_store();
    auto const option = KTRY( ostore.install_option( heading, descr, value, action ) );

    options.emplace_back( option );

    rv = option;

    return rv;
}

} // namespace kmap::option
