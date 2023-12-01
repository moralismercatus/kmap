/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <kmap.hpp>
#include <util/fuzzy_search/fuzzy_search.hpp>

#include <emscripten/bind.h>
#include <fmt/format.h>

namespace kmap {
namespace binding {
namespace {

using namespace emscripten;

auto fuzzy_search_titles( std::string const& query )
    -> void
{
    auto const& km = kmap::Singleton::instance();

    auto const results =  util::fuzzy_search_titles( km, query );
}

EMSCRIPTEN_BINDINGS( kmap_fuzzy_search )
{
    function( "fuzzy_search_titles", &kmap::binding::fuzzy_search_titles );
}

} // namespace anon
} // namespace binding
} // namespace kmap
