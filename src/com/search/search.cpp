/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/search/search.hpp>

#include <util/fuzzy_search/fuzzy_search.hpp>

namespace kmap::com {

Search::Search( Kmap& kmap
              , std::set< std::string > const& requisites
              , std::string const& description )
    : Component{ kmap, requisites, description }
    , cclerk_{ kmap }
    , oclerk_{ kmap }
{
    KM_RESULT_PROLOG();

    KTRYE( register_standard_commands() );
}

auto Search::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    
    KTRY( cclerk_.install_registered() );
    KTRY( oclerk_.install_registered() );

    rv = outcome::success();

    return rv;
}

auto Search::load()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( cclerk_.check_registered() );
    KTRY( oclerk_.check_registered() );
    
    rv = outcome::success();

    return rv;
}

auto Search::register_standard_commands()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    // search.title
    {
        auto const action_code =
        R"%%%(
            const search = kmap.search();

            ktry( search.search_titles( args.get( 0 ) ) );
        )%%%";
        auto const description = "searches titles for text matching argument";
        auto const arguments = std::vector< Command::Argument >{ Command::Argument{ "title_query"
                                                                                  , "title query text"
                                                                                  , "unconditional" } };

        KTRY( cclerk_.register_command( com::Command{ .path = "search.title"
                                                    , .description = description
                                                    , .arguments = arguments 
                                                    , .guard = "unconditional"
                                                    , .action = action_code } ) );
    }

    rv = outcome::success();

    return rv;
}

auto Search::register_standard_options()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();

    // search.result.limit
    {
        auto const action_code =
        R"%%%(
            const search = kmap.search();

            ktry( search.search_titles( args.get( 0 ) ) );
        )%%%";
        auto const description = "searches titles for text matching argument";
        auto const arguments = std::vector< Command::Argument >{ Command::Argument{ "title_query"
                                                                                  , "title query text"
                                                                                  , "unconditional" } };

        KTRY( oclerk_.register_option( com::Option{ .heading = "search.result.limit"
                                                  , .descr = "Maximum number of results returned from a search."
                                                  , .value = "25"
                                                  , .action = "kmap.search().set_limit( option_value );"  } ) );
    }

    rv = outcome::success();

    return rv;
}

auto Search::search_titles( std::string const& query )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< void >();
    auto const titles = util::fuzzy_search_titles( kmap_inst(), query, 25 );

    rv = outcome::success();

    return rv;
}

namespace {
namespace search_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::Search
,   std::set({ "command.store"s, "command.standard_items"s })
,   "search functionality"
);

} // namespace tag_store_def 
}

} // namespace kmap::com