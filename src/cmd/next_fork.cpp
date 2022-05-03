/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "next_fork.hpp"

#include "../contract.hpp"
#include "../common.hpp"
#include "../io.hpp"
#include "../kmap.hpp"

namespace kmap::cmd {

auto next_fork( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 );
            })
        ;

        auto const target = [ & ]() -> Optional< Uuid >
        {
            auto rv = Optional< Uuid >{};
            auto n = kmap.selected_node();

            while( !rv )
            {
                auto const children = kmap.fetch_children( n );

                if( children.empty() )
                {
                    break;
                }
                else if( children.size() == 1 )
                {
                    n = *children.begin();
                }
                else
                {
                    rv = n;
                }
            }

            return rv;
        }();

        if( target )
        {
            kmap.jump_to( *target ).value();

            return fmt::format( "selected next fork" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "no next fork found" ) );
        }
    };
}

auto prev_fork( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 );
            })
        ;

        auto const target = [ & ]() -> Optional< Uuid >
        {
            auto rv = Optional< Uuid >{};
            auto n = kmap.selected_node();

            while( !rv )
            {
                auto const parent = kmap.fetch_parent( n );

                if( !parent )
                {
                    break;
                }
                else
                {
                    auto const children = kmap.fetch_children( parent.value() );

                    if( children.size() > 1 )
                    {
                        rv = to_optional( parent );
                    }
                    else
                    {
                        n = parent.value();
                    }
                }
            }

            return rv;
        }();

        if( target )
        {
            kmap.jump_to( *target ).value();

            return fmt::format( "selected previous fork" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "no previuous fork found" ) );
        }
    };
}

auto next_leaf( Kmap& kmap )
    -> std::function< Result< std::string >( CliCommand::Args const& args ) >
{
    return [ &kmap ]( CliCommand::Args const& args ) -> Result< std::string >
    {
        BC_CONTRACT()
            BC_PRE([ & ]
            {
                BC_ASSERT( args.size() == 0 );
            })
        ;

        return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "impl. needed" ) );

        auto const target = [ & ]() -> Optional< Uuid >
        {
            auto rv = Optional< Uuid >{};
            auto n = kmap.selected_node();

            while( !rv )
            {
                auto const children = kmap.fetch_children( n );

                if( children.empty() )
                {
                    break;
                }
                else if( children.size() == 1 )
                {
                    n = *children.begin();
                }
                else
                {
                    rv = n;
                }
            }

            return rv;
        }();

        if( target )
        {
            kmap.jump_to( *target ).value();

            return fmt::format( "selected next leaf" );
        }
        else
        {
            return KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, fmt::format( "no next leaf found" ) );
        }
    };
}

} // namespace kmap::cmd