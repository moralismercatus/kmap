/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/search/search.hpp>

#include <memory>

namespace kmap::com {

namespace binding
{
    using namespace emscripten;

    auto search()
    {
        KM_RESULT_PROLOG();

        auto& km = kmap::Singleton::instance();

        return KTRYE( km.fetch_component< com::Search >() );
    }

    EMSCRIPTEN_BINDINGS( kmap_com_search )
    {
        function( "search", &kmap::com::binding::search );
        class_< kmap::com::Search >( "Search" )
            .smart_ptr< std::shared_ptr< kmap::com::Search > >( "Search" )
            .function( "search_titles", &kmap::com::Search::search_titles )
            ;
        
    }
} // namespace binding

} // namespace kmap::com
