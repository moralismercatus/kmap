/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "util/clerk/clerk.hpp"

#include "cmd/parser.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/act/fetch_body.hpp"
#include "path/node_view.hpp"
#include "util/result.hpp"


#include <fmt/format.h>

namespace kmap::util {

auto confirm_reinstall( std::string const& item
                      , std::string const& path )
    -> Result< bool >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "item", item );
        KM_RESULT_PUSH_STR( "path", path );

    return KTRY( js::eval< bool >( fmt::format( "return confirm( \"{0} '{1}' out of date.\\nRe-install {0}?\" );", item, path ) ) );
}

auto match_body_code( Kmap const& km
                    , view2::Tether const& tether
                    , std::string const& content )
    -> bool
{
    auto rv = false;

    if( auto const body = tether | act2::fetch_body( km )
      ; body )
    {
        auto const code = KTRYB( cmd::parser::parse_body_code( body.value() ) );

        boost::apply_visitor( [ & ]( auto const& e )
                            {
                                using T = std::decay_t< decltype( e ) >;

                                if constexpr( std::is_same_v< T, cmd::ast::Kscript > )
                                {
                                    // TODO: Beautify kscript? To make comparison consistent regardless of syntax....
                                    //       Or, tokenize kscript and compare. May be better.
                                    rv = ( content == e.code );
                                }
                                else if constexpr( std::is_same_v< T, cmd::ast::Javascript > )
                                {
                                    rv = ( js::beautify( content ) == js::beautify( e.code ) ); 
                                }
                                else
                                {
                                    static_assert( always_false< T >::value, "non-exhaustive visitor!" );
                                }
                            }
                            , code );
    }

    return rv;
}

auto match_raw_body( Kmap const& km
                   , view2::Tether const& tether
                   , std::string const& content )
    -> bool
{
    auto rv = false;

    if( auto const body = tether | act2::fetch_body( km )
      ; body )
    {
        rv = ( body.value() == content );
    }

    return rv;
}

}  // namespace kmap::util
