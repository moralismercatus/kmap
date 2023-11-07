/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/tag/tag.hpp>

#include <emscripten.h>

namespace kmap::com {

namespace binding {

using namespace emscripten;

struct TagStore
{
    Kmap& kmap_;

    TagStore( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto create_tag( std::string const& path )
        -> kmap::Result< Uuid >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_STR( "path", path );

        auto const tstore = KTRY( kmap_.fetch_component< com::TagStore >() );

        return tstore->create_tag( path );
    }
    auto erase_tag( Uuid const& node
                  , std::string const& tpath )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_NODE( "node", node );
            KM_RESULT_PUSH_STR( "path", tpath );

        auto rv = KMAP_MAKE_RESULT( void );
        auto const tstore = KTRY( kmap_.fetch_component< com::TagStore >() );

        KTRY( view::make( node )
            | view::tag( tpath )
            | view::erase_node( kmap_ ) );

        rv = outcome::success();

        return rv;
    }
    auto tag_node( Uuid const& node
                 , std::string const& path )
        -> kmap::Result< Uuid >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_NODE( "node", node );
            KM_RESULT_PUSH_STR( "path", path );

        auto const tstore = KTRY( kmap_.fetch_component< com::TagStore >() );

        return tstore->tag_node( node, path );
    }
};

auto tag_store()
    -> binding::TagStore
{
    auto& kmap = Singleton::instance();

    return binding::TagStore{ kmap };
}

EMSCRIPTEN_BINDINGS( kmap_tag_store )
{
    function( "tag_store", &kmap::com::binding::tag_store );
    class_< kmap::com::binding::TagStore >( "TagStore" )
        .function( "create_tag", &kmap::com::binding::TagStore::create_tag )
        .function( "erase_tag", &kmap::com::binding::TagStore::erase_tag )
        .function( "tag_node", &kmap::com::binding::TagStore::tag_node )
        ;
}

} // namespace binding

} // namespace kmap::com