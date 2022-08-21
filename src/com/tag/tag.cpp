/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "tag.hpp"

#include "com/cmd/cclerk.hpp"
#include "com/network/network.hpp"
#include "common.hpp" 
#include "emcc_bindings.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "path/node_view.hpp"
#include "test/util.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

namespace kmap::com {

TagStore::TagStore( Kmap& kmap
                  , std::set< std::string > const& requisites
                  , std::string const& description )
    : Component{ kmap, requisites, description }
    , cclerk_{ kmap }
{
}

auto TagStore::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    
    KTRY( install_standard_commands() );

    rv = outcome::success();

    return rv;
}

auto TagStore::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    
    rv = outcome::success();

    return rv;
}

auto TagStore::install_standard_commands()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // arg.tag_path
    {
        auto const guard_code =
R"%%%(```javascript
return kmap.is_valid_heading_path( arg );
```)%%%";
        auto const completion_code =
R"%%%(```javascript
const troot = kmap.fetch_node( "/meta.tag" );

if( troot.has_error() )
{
    return new kmap.VectorString();
}
else
{
    return kmap.complete_heading_path_from( troot.value(), troot.value(), arg );
}
```)%%%";

        auto const description = "tag heading path";
        
        KTRY( cclerk_.install_argument( com::Argument{ .path = "tag_path"
                                                     , .description = description
                                                     , .guard = guard_code
                                                     , .completion = completion_code } ) );
    }

    using Guard = com::Command::Guard;
    using Argument = com::Command::Argument;

    // create.tag
    {
        auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
        auto const action_code =
R"%%%(```javascript
const tagn = kmap.tag_store().create_tag( args.get( 0 ) );

if( tagn.has_value() )
{
    kmap.select_node( tagn.value() );

    return kmap.success( 'success' );
}
else
{
    return kmap.failure( tagn.error_message() );
}
```)%%%";
        auto const description = "creates tag";
        // TODO: err.. need argument created, prior to calling install_command, but that's done in task_store, ATM.
        auto const arguments = std::vector< Argument >{ Argument{ "tag_title"
                                                                , "tag title"
                                                                , "unconditional" } };

        KTRY( cclerk_.install_command( com::Command{ .path = "create.tag"
                                                   , .description = description
                                                   , .arguments = arguments 
                                                   , .guard = Guard{ "unconditional", guard_code }
                                                   , .action = action_code } ) );
    }
    // tag.node
    {
        auto const guard_code =
R"%%%(```javascript
return kmap.success( 'unconditional' );
```)%%%";
        auto const action_code =
R"%%%(```javascript
kmap.tag_store().tag_node( kmap.selected_node(), args.get( 0 ) ).throw_on_error();

kmap.select_node( kmap.selected_node() );

return kmap.success( 'success' );
```)%%%";
        auto const description = "appends tag to node";
        auto const arguments = std::vector< Argument >{ Argument{ "tag_path"
                                                                , "path to target tag"
                                                                , "tag_path" } };

        KTRY( cclerk_.install_command( com::Command{ .path = "tag.node"
                                                   , .description = description
                                                   , .arguments = arguments 
                                                   , .guard = Guard{ "unconditional", guard_code }
                                                   , .action = action_code } ) );
    }

    rv = outcome::success();

    return rv;
}

auto TagStore::fetch_tag_root() const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    rv = KTRY( view::make( km.root_node_id() )
             | view::direct_desc( "meta.tag" )
             | view::fetch_node( km ) );

    return rv;
}

auto TagStore::fetch_tag_root()
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();

    rv = KTRY( view::make( km.root_node_id() )
             | view::direct_desc( "meta.tag" )
             | view::fetch_or_create_node( km ) );

    return rv;
}

auto TagStore::create_tag( std::string const& path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const troot = KTRY( fetch_tag_root() );

    rv = KTRY( view::make( troot )
             | view::direct_desc( path )
             | view::create_node( km )
             | view::to_single );

    return rv;
}

auto TagStore::fetch_tag( std::string const& tag_path ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const troot = KTRY( fetch_tag_root() );

    rv = KTRY( view::make( troot )
             | view::desc( tag_path )
             | view::fetch_node( km ) );

    return rv;
}

auto TagStore::fetch_or_create_tag( std::string const& path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const troot = KTRY( fetch_tag_root() );

    rv = KTRY( view::make( troot )
             | view::direct_desc( path )
             | view::fetch_or_create_node( km ) );

    return rv;
}

SCENARIO( "create_tag", "[tag]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "tag_store" );

    auto& kmap = Singleton::instance();
    auto const tstore = REQUIRE_TRY( kmap.fetch_component< TagStore >() );
    auto const nw = REQUIRE_TRY( kmap.fetch_component< com::Network >() );

    GIVEN( "no tags" )
    {
        WHEN( "create one level tag" )
        {
            REQUIRE_RES( tstore->create_tag( "alpha" ) );

            THEN( "tag exists" )
            {
                REQUIRE( nw->exists( "/meta.tag.alpha" ) );
            }
        }
        WHEN( "create multi-level tag" )
        {
            REQUIRE_RES( tstore->create_tag( "alpha.beta" ) );

            THEN( "tag exists" )
            {
                REQUIRE( nw->exists( "/meta.tag.alpha.beta" ) );
            }
        }
    }
}

auto TagStore::has_tag( Uuid const& node
                      , Uuid const& tag ) const
    -> bool
{
    auto rv = false;
    auto& km = kmap_inst();
    auto const nts = view::make( node )
                    | view::attr
                    | view::child( "tag" )
                    | view::alias
                    | view::resolve
                    | view::to_node_set( km );

    rv = nts.contains( tag );

    return rv;
}

auto TagStore::has_tag( Uuid const& node
                      , std::string const& tag_path ) const
    -> bool
{
    auto rv = false;

    if( auto const tag = fetch_tag( tag_path )
      ; tag )
    {
        rv = has_tag( node, tag.value() );
    }

    return rv;
}

// TODO: What are the ramifications of aliasing a non-attr into a the attr tree? Is it ok?
auto TagStore::tag_node( Uuid const& target 
                       , Uuid const& tag )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const troot = KTRY( fetch_tag_root() );
    auto const rtag = nw->alias_store().resolve( tag ); // TODO: Verify resolve() is the right thing to do. Use case: when tagging nodes based on other tags (which are aliases).;

    KMAP_ENSURE( is_ancestor( km, troot, rtag ), error_code::network::invalid_lineage );

    rv = KTRY( view::make( target )
             | view::attr
             | view::child( "tag" )
             | view::alias( view::Alias::Src{ rtag } )
             | view::create_node( km )
             | view::to_single );

    return rv;
}

auto TagStore::tag_node( Uuid const& target 
                       , std::string const& tag_path )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto& km = kmap_inst();
    auto const troot = KTRY( fetch_tag_root() );
    auto const tag = KTRY( view::make( troot )
                         | view::desc( tag_path )
                         | view::fetch_node( km ) );

    rv = KTRY( tag_node( target, tag ) );

    return rv;
}

SCENARIO( "tag_node", "[tag]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "tag_store", "alias_store" );

    auto& km = Singleton::instance();
    auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    auto const tstore = REQUIRE_TRY( km.fetch_component< com::TagStore >() );

    GIVEN( "tag exists" )
    {
        auto const atag = REQUIRE_TRY( tstore->create_tag( "alpha" ) );

        GIVEN( "target node" )
        {
            auto const tnode = REQUIRE_TRY( nw->create_child( km.root_node_id(), "1" ) );

            WHEN( "tag node" )
            {
                auto const dtag = REQUIRE_TRY( tstore->tag_node( tnode, "alpha" ) );

                THEN( "node tagged" )
                {
                    auto const attr_tag = REQUIRE_TRY( view::make( tnode )
                                        | view::attr
                                        | view::direct_desc( "tag.alpha" )
                                        | view::fetch_node( km ) );

                    REQUIRE( attr_tag == dtag );
                    REQUIRE( nw->alias_store().resolve( attr_tag ) == atag );
                }
            }
        }
    }
}

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
        -> kmap::binding::Result< Uuid >
    {
        auto const tstore = KTRY( kmap_.fetch_component< com::TagStore >() );

        return tstore->create_tag( path );
    }
    auto tag_node( Uuid const& node
                 , std::string const& path )
        -> kmap::binding::Result< Uuid >
    {
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
        .function( "tag_node", &kmap::com::binding::TagStore::tag_node )
        ;
}

} // namespace binding

namespace {
namespace tag_store_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::TagStore
,   std::set({ "command_store"s })
,   "tag related functionality"
);

} // namespace tag_store_def 
}

} // namespace kmap::com