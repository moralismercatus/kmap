/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/common.hpp>
#include <com/cli/cli.hpp>
#include <com/database/db.hpp>
#include <com/database/filesystem/db_fs.hpp>
#include <com/event/event.hpp>
#include <com/filesystem/filesystem.hpp>
#include <component.hpp>
#include <js/iface.hpp>
#include <test/util.hpp>

#include <boost/filesystem.hpp>
#include <catch2/catch_test_macros.hpp>

namespace kmap {

SCENARIO( "Erase canvas node within save/load", "[canvas][db][fs][save][load]" )
{
    auto& km = Singleton::instance();
    auto const db = [ & ] { return REQUIRE_TRY( km.fetch_component< com::Database >() ); };

    GIVEN( "all components initialized" )
    {
        auto const all_components = REQUIRE_TRY( fetch_listed_components() );

        KMAP_COMPONENT_FIXTURE_SCOPED( all_components );

        auto const disk_path = ".db_fs_test.kmap";
        auto const abs_disk_path = com::kmap_root_dir / disk_path;

        GIVEN( "erase canvas root node" )
        {
            REQUIRE(( view2::canvas::canvas_root | act2::exists( km ) ));

            REQUIRE_TRY(( view2::canvas::canvas_root
                        | act2::erase_node( km ) ));

            GIVEN( "init_db_on_disk" )
            {
                REQUIRE_TRY( db()->init_db_on_disk( abs_disk_path ) );

                GIVEN( "flush_delta_to_disk" )
                {
                    REQUIRE_RES( db()->flush_delta_to_disk() );

                    GIVEN( "load" )
                    {
                        KMAP_LOG_LINE();
                        REQUIRE_RES( km.load( disk_path, all_components ) );
                        KMAP_LOG_LINE();

                        THEN( "enter CLI command bar" )
                        {
                            auto const estore = REQUIRE_TRY( km.fetch_component< com::EventStore >() );

                            // Trigger command bar focus.
                            REQUIRE_TRY( estore->fire_event( { "subject.window", "verb.depressed", "object.keyboard.key.shift" } ) );
                            REQUIRE_TRY( estore->fire_event( { "subject.window", "verb.depressed", "object.keyboard.key.colon" } ) );

                            THEN( "execute command" )
                            {
                                auto const cli = REQUIRE_TRY( km.fetch_component< com::Cli >() );

                                REQUIRE_TRY( cli->on_key_down( 13 // enter
                                                             , false
                                                             , false
                                                             , ":@root" ) );
                            }
                        }
                    }
                }
            }
        }

        if( exists( abs_disk_path ) ) { boost::filesystem::remove( abs_disk_path ); }
    }
}

SCENARIO( "Javascript preprocessed to transform ktry()", "[js_iface]" )
{
    {
        auto const pre = "ktry( kmap.uuid_from_string( \"0x0\" ) );";
        auto const post = "{\n  const ktry_postproc_temp_val = kmap.uuid_from_string( \"0x0\" );\n\n  if (ktry_postproc_temp_val.has_error()) {\n    return ktry_postproc_temp_val.error();\n  }\n};";
        REQUIRE( post == REQUIRE_TRY( js::eval< std::string >( fmt::format( "return kmap_preprocess_js_script( '{}' );", pre ) ) ) );
    }
    {
        auto const pre = "foo( ktry( kmap.uuid_from_string( \"0x0\" ) ) );";
        auto const post = "{\n  const ktry_postproc_temp_val = kmap.uuid_from_string( \"0x0\" );\n\n  if (ktry_postproc_temp_val.has_error()) {\n    return ktry_postproc_temp_val.error();\n  }\n\n  foo(ktry_postproc_temp_val.value());\n};";
        REQUIRE( post == REQUIRE_TRY( js::eval< std::string >( fmt::format( "return kmap_preprocess_js_script( '{}' );", pre ) ) ) );
    }
    {
        auto const pre = "x = ktry( kmap.uuid_from_string( \"0x0\" ) );";
        auto const post = "{\n  const ktry_postproc_temp_val_x = kmap.uuid_from_string( \"0x0\" );\n\n  if (ktry_postproc_temp_val_x.has_error()) {\n    return ktry_postproc_temp_val_x.error();\n  }\n\n  x = ktry_postproc_temp_val_x.value();\n};";
        REQUIRE( post == REQUIRE_TRY( js::eval< std::string >( fmt::format( "return kmap_preprocess_js_script( '{}' );", pre ) ) ) );
    }
    {
        auto const pre = "const x = ktry( kmap.uuid_from_string( \"0x0\" ) );";
        auto const post = "const ktry_postproc_temp_val_x = kmap.uuid_from_string( \"0x0\" );\n\nif (ktry_postproc_temp_val_x.has_error()) {\n  return ktry_postproc_temp_val_x.error();\n}\n\nconst x = ktry_postproc_temp_val_x.value();";
        REQUIRE( post == REQUIRE_TRY( js::eval< std::string >( fmt::format( "return kmap_preprocess_js_script( '{}' );", pre ) ) ) );
    }
}

SCENARIO( "instanceof", "[js_iface]" )
{
    using namespace emscripten;

    // OK: Found the ticket. val::global() does not accept subitems e.g., kmap.Uuid, kmap.success, etc. You can use index operator to access down levels.
    {
        auto const g = val::global( "kmap" );
        REQUIRE( g.as< bool >() );
    }
    {
        auto const g = val::global( "lint_javascript" );
        REQUIRE( g.as< bool >() );
    }
    {
        auto const g = val::global( "kmap" )[ "Uuid" ];
        REQUIRE( g.as< bool >() );
    }
    // {
    //     auto const g = val::global( "success" );
    //     REQUIRE( g.as< bool >() );
    // }
    {
        auto const g = val::global( "kmap" )[ "success" ];
        REQUIRE( g.as< bool >() );
    }
    {
        auto const g = val::global( "kmap" )[ "Uuid" ];
        REQUIRE( g.as< bool >() );
    }
    {
        auto const g = val::global( "kmap" )[ "result$$Payload" ];
        REQUIRE( g.as< bool >() );
    }

    KMAP_COMPONENT_FIXTURE_SCOPED( "canvas" );

    // auto const pp = REQUIRE_TRY( js::preprocess( "ktry( kmap.canvas().apply_layout('fail') ); return 'ok';" ) );
    auto const pp = REQUIRE_TRY( js::preprocess( "ktry( kmap.failure2( 'test' ) );" ) );
    REQUIRE_RFAIL( js::eval< std::string >( pp ) );
}

} // namespace kmap
