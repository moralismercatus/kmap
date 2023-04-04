/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/option/option_clerk.hpp"

#include "com/option/option.hpp"
#include "error/master.hpp"
#include "kmap.hpp"
#include "util/clerk/clerk.hpp"
#include "util/result.hpp"

#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>

namespace rvs = ranges::views;

namespace kmap::com {

OptionClerk::OptionClerk( Kmap& km )
    : kmap_{ km }
{
}

OptionClerk::~OptionClerk()
{
    try
    {
        if( auto const ostorec = kmap_.fetch_component< com::OptionStore >()
          ; ostorec )
        {
            auto& ostore = ostorec.value();
            for( auto const& e : installed_options_
                               | rvs::values
                               | rvs::reverse )
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

auto OptionClerk::apply_installed()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const ostore = KTRY( kmap_.fetch_component< com::OptionStore >() );

    for( auto const& opt : installed_options_
                         | rvs::values )
    {
        KTRY( ostore->apply( opt ) );
    }

    rv = outcome::success();

    return rv;
}

auto OptionClerk::check_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const ostore = KTRY( kmap_.fetch_component< com::OptionStore >() );
    auto const voroot = view::abs_root | view::direct_desc( "meta.setting.option" );

    for( auto const& option : registered_options_
                            | rvs::values )
    {
        if( auto const optn = voroot 
                           | view::direct_desc( option.heading )
                           | view::fetch_node( kmap_ )
          ; optn )
        {
            auto const matches = util::match_body_code( kmap_, view::make( optn.value() ) | view::child( "action" ), option.action )
                              && util::match_raw_body( kmap_, view::make( optn.value() ) | view::child( "description" ), option.descr )
                              && util::match_raw_body( kmap_, view::make( optn.value() ) | view::child( "value" ), option.value )
                               ;

            if( !matches )
            {
                if( util::confirm_reinstall( "Option", option.heading ) )
                {
                    KTRY( view::make( optn.value() )
                        | view::erase_node( kmap_ ) );
                    KTRY( install_option( option ) ); // Re-install outlet.
                }
            }
            else
            {
                installed_options_.insert( { option.heading, optn.value() } );
            }
        }
        else
        {
            if( util::confirm_reinstall( "Option", option.heading ) )
            {
                KTRY( install_option( option ) );
            }
        }
    }

    rv = outcome::success();

    return rv;
}

auto OptionClerk::register_option( Option const& option )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    
    KMAP_ENSURE( !registered_options_.contains( option.heading ), error_code::common::uncategorized );

    registered_options_.insert( { option.heading, option } );

    rv = outcome::success();

    return rv;
}

auto OptionClerk::install_option( Option const& option )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", option.heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const ostore = KTRY( kmap_.fetch_component< com::OptionStore >() );
    auto const optn = KTRY( ostore->install_option( option ) );

    installed_options_.insert( { option.heading, optn } );

    rv = optn;

    return rv;
}

auto OptionClerk::install_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& opt : registered_options_
                         | ranges::views::values )
    {
        KTRY( install_option( opt ) );
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap::com
