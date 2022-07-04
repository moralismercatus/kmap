/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "tag.hpp"

#include "common.hpp" 
#include "kmap.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap {

auto fetch_tag_root( Kmap const& kmap )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( view::make( kmap.root_node_id() )
             | view::direct_desc( "meta.tag" )
             | view::fetch_node( kmap ) );

    return rv;
}

auto fetch_tag_root( Kmap& kmap )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    rv = KTRY( view::make( kmap.root_node_id() )
             | view::direct_desc( "meta.tag" )
             | view::fetch_or_create_node( kmap ) );

    return rv;
}

auto create_tag( Kmap& kmap
               , std::string const& path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const troot = KTRY( fetch_tag_root( kmap ) );

    rv = KTRY( view::make( troot )
             | view::direct_desc( path )
             | view::create_node( kmap )
             | view::to_single );

    return rv;
}

auto fetch_tag( Kmap const& kmap
              , std::string const& tag_path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const troot = KTRY( fetch_tag_root( kmap ) );

    rv = KTRY( view::make( troot )
             | view::desc( tag_path )
             | view::fetch_node( kmap ) );

    return rv;
}

auto fetch_or_create_tag( Kmap& kmap
                        , std::string const& path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const troot = KTRY( fetch_tag_root( kmap ) );

    rv = KTRY( view::make( troot )
             | view::direct_desc( path )
             | view::fetch_or_create_node( kmap ) );

    return rv;
}

SCENARIO( "create_tag", "[tag]" )
{
    KMAP_DATABASE_ROOT_FIXTURE_SCOPED();
    auto& kmap = Singleton::instance();

    GIVEN( "no tags" )
    {
        WHEN( "create one level tag" )
        {
            REQUIRE_RES( create_tag( kmap, "alpha" ) );

            THEN( "tag exists" )
            {
                REQUIRE( kmap.exists( "/meta.tag.alpha" ) );
            }
        }
        WHEN( "create multi-level tag" )
        {
            REQUIRE_RES( create_tag( kmap, "alpha.beta" ) );

            THEN( "tag exists" )
            {
                REQUIRE( kmap.exists( "/meta.tag.alpha.beta" ) );
            }
        }
    }
}

auto has_tag( Kmap const& kmap
            , Uuid const& node
            , Uuid const& tag )
    -> bool
{
    auto rv = false;
    auto const nts = view::make( node )
                    | view::attr
                    | view::child( "tag" )
                    | view::alias
                    | view::resolve
                    | view::to_node_set( kmap );

    rv = nts.contains( tag );

    return rv;
}

auto has_tag( Kmap const& kmap
            , Uuid const& node
            , std::string const& tag_path )
    -> bool
{
    auto rv = false;

    if( auto const tag = fetch_tag( kmap, tag_path )
      ; tag )
    {
        rv = has_tag( kmap, node, tag.value() );
    }

    return rv;
}

// TODO: What are the ramifications of aliasing a non-attr into a the attr tree? Is it ok?
auto tag_node( Kmap& kmap
             , Uuid const& target 
             , Uuid const& tag )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const troot = KTRY( fetch_tag_root( kmap ) );
    auto const rtag = kmap.resolve( tag ); // TODO: Verify resolve() is the right thing to do. Use case: when tagging nodes based on other tags (which are aliases).;

    KMAP_ENSURE( kmap.is_ancestor( troot, rtag ), error_code::network::invalid_lineage );

    rv = KTRY( view::make( target )
             | view::attr
             | view::child( "tag" )
             | view::alias( view::Alias::Src{ rtag } )
             | view::create_node( kmap )
             | view::to_single );

    return rv;
}

auto tag_node( Kmap& kmap
             , Uuid const& target 
             , std::string const& tag_path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const troot = KTRY( fetch_tag_root( kmap ) );
    auto const tag = KTRY( view::make( troot )
                         | view::desc( tag_path )
                         | view::fetch_node( kmap ) );

    rv = KTRY( tag_node( kmap, target, tag ) );

    return rv;
}

SCENARIO( "tag_node", "[tag]" )
{
    KMAP_DATABASE_ROOT_FIXTURE_SCOPED();

    auto& kmap = Singleton::instance();

    GIVEN( "tag exists" )
    {
        auto const atag = REQUIRE_TRY( create_tag( kmap, "alpha" ) );

        GIVEN( "target node" )
        {
            auto const tnode = REQUIRE_TRY( kmap.create_child( kmap.root_node_id(), "1" ) );

            WHEN( "tag node" )
            {
                auto const dtag = REQUIRE_TRY( tag_node( kmap, tnode, "alpha" ) );

                THEN( "node tagged" )
                {
                    auto const attr_tag = REQUIRE_TRY( view::make( tnode )
                                        | view::attr
                                        | view::direct_desc( "tag.alpha" )
                                        | view::fetch_node( kmap ) );

                    REQUIRE( attr_tag == dtag );
                    REQUIRE( kmap.resolve( attr_tag ) == atag );
                }
            }
        }
    }
}

} // namespace kmap