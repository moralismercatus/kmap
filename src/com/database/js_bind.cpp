/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/database/db.hpp>
#include <com/filesystem/filesystem.hpp>

#include <emscripten/bind.h>

namespace kmap::com {

namespace binding {

using namespace emscripten;

struct Database
{
    Kmap& kmap_;

    Database( Kmap& kmap )
        : kmap_{ kmap }
    {
    }

    auto init_db_on_disk( std::string const& path )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();
            KM_RESULT_PUSH_STR( "path", path );

        auto const db = KTRY( kmap_.fetch_component< com::Database >() );

        return db->init_db_on_disk( com::kmap_root_dir / path );
    }

    auto flush_delta_to_disk()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        try
        {
            auto const db = KTRY( kmap_.fetch_component< com::Database >() );

            return db->flush_delta_to_disk();
        }
        catch( std::exception const& e )
        {
            return kmap::Result< void >{ KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, e.what() ) };
        }
    }

    auto flush_cache_to_disk()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        try
        {
            auto const db = KTRY( kmap_.fetch_component< com::Database >() );

            return db->flush_cache_to_disk();
        }
        catch( std::exception const& e )
        {
            return kmap::Result< void >{ KMAP_MAKE_ERROR_MSG( error_code::common::uncategorized, e.what() ) };
        }
    }

    auto has_delta()
        -> bool
    {
        KM_RESULT_PROLOG();

        auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

        return db->has_delta();
    }

    auto has_file_on_disk()
        -> bool
    {
        KM_RESULT_PROLOG();

        auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

        return db->has_file_on_disk();
    }

    auto path()
        -> Result< std::string >
    {
        KM_RESULT_PROLOG();

        auto const db = KTRYE( kmap_.fetch_component< com::Database >() );

        return KTRY( db->path() ).string();
    }
};

auto database()
    -> binding::Database
{
    return binding::Database{ Singleton::instance() };
}

EMSCRIPTEN_BINDINGS( kmap_database )
{
    function( "database", &kmap::com::binding::database );
    class_< kmap::com::binding::Database >( "Database" )
        .function( "init_db_on_disk", &kmap::com::binding::Database::init_db_on_disk )
        .function( "flush_delta_to_disk", &kmap::com::binding::Database::flush_delta_to_disk )
        .function( "flush_cache_to_disk", &kmap::com::binding::Database::flush_cache_to_disk )
        .function( "has_delta", &kmap::com::binding::Database::has_delta )
        .function( "has_file_on_disk", &kmap::com::binding::Database::has_file_on_disk )
        .function( "path", &kmap::com::binding::Database::path )
        ;
}

} // namespace binding
} // namespace kmap::com
