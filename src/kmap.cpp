/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "kmap.hpp"

#include "com/canvas/canvas.hpp"
#include "com/database/db.hpp"
#include "com/database/root_node.hpp"
#include "com/event/event.hpp"
#include "com/filesystem/filesystem.hpp"
#include "com/network/network.hpp"
#include "com/text_area/text_area.hpp"
#include "common.hpp"
#include "component.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "util/result.hpp"

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace kmap {

Kmap::Kmap()
{
    io::print( "in kmap ctor\n" );
}

auto Kmap::database_path() const
    -> FsPath const&
{
    return database_path_;
}

auto Kmap::root_node_id() const
    -> Uuid const&
{
    auto const dbr = KTRYE( fetch_component< com::RootNode >() );

    return dbr->root_node();
}

auto Kmap::component_store()
    -> ComponentStore&
{
    KMAP_ENSURE_EXCEPT( component_store_ );

    return *component_store_;
}

auto Kmap::component_store() const
    -> ComponentStore const&
{
    KMAP_ENSURE_EXCEPT( component_store_ );

    return *component_store_;
}

#if 0
// TODO: This is not efficiently implemented. The data is not streamed into the
// DB, but rather copied twice - once into a compressed buffer and then again
// into a uint8_t buffer for consumption by the DB.
auto Kmap::store_resource( Uuid const& parent
                         , Heading const& heading
                         , std::byte const* data
                         , size_t const& size )
    -> Result< Uuid >
{
    KMAP_THROW_EXCEPTION_MSG( "TODO" );
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( database().node_exists( resolve( parent ) ) ); // TODO: this function should return an outcome rather than fail with precondition.
            BC_ASSERT( data != nullptr );
            BC_ASSERT( size != 0 );
        })
    ;

    auto const hash = gen_md5_uuid( data
                                  , size );
    auto const shash = to_string( hash );

    // TODO: Why is it not required to format the heading from a title? Isn't it xxx-xxx-...?
    // create_child( shash | views::replace( '-'
    //                                 , '_' )
    KMAP_TRY( create_child( parent
                          , hash
                          , heading ) );

    {
        auto const cr = compress_resource( data
                                         , size );
        auto const db = KTRY( fetch_component< com::Database >() );
        // auto orst = original_resource_sizes::original_resource_sizes{};
        auto rt = resources::resources{};

        // db.execute( insert_into( orst )
        //           . set( orst.uuid = shash
        //                , orst.resource_size = size ) );
        db->execute( insert_into( rt )
                   . set( rt.uuid = shash
                        , rt.resource = std::vector< uint8_t >{ reinterpret_cast< uint8_t const* >( cr.data() )
                                                              , reinterpret_cast< uint8_t const* >( cr.data() + cr.size() ) } ) );
    }

    return hash;
}
#endif // 0

auto Kmap::init_component_store()
    -> void
{
    component_store_ = std::make_unique< ComponentStore >( *this );
}

auto Kmap::initialize()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    {
        init_component_store();
        KTRY( register_all_components() );
        KTRY( component_store_->fire_initialized( "component_store" ) );
    }

    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        KTRY( estore.value()->fire_event( { "verb.initialized", "object.kmap" } ) );
    }

    {
        // TODO: 'shutdown' component? Definitely.
        //       I can add a flag to kmap.js to disable check-before-close events for dev purposes.
#if 0 // Temporarily disabling. Possibly updating Electron caused difference in onbeforeunload behavior.
        auto const outlet_script =
        R"%%%(
        console.log( '[outlet.confirm_shutdown] Shutdown event triggered.' );
        console.log( '[outlet.confirm_shutdown] TODO: Here is where the confirmation dialog goes.' );
        console.log( '[outlet.confirm_shutdown] TODO: Here is where the unsaved changes check goes.' );
        console.log( '[outlet.confirm_shutdown] Waiting 2 second before proceeding with shutdown, so these console logs show.' );
        console.log( '[outlet.confirm_shutdown] TODO: Follow-throw disabled to allow reloads. Perform operation again to get effect desired.' );
        setTimeout(function()
        {
            // Returning undefined allows close/shutdown to proceed.
            window.onbeforeunload = function(e){{ return undefined; }};
            // window.close(); // TODO: Need to handle reload events as well.
        }, 2000);

        )%%%";

        KMAP_TRY( estore.install_subject( "kmap" ) );
        KMAP_TRY( estore.install_verb( "shutdown" ) );
        KMAP_TRY( estore.install_outlet( "confirm_shutdown"
                                       , Transition::Leaf{ .requisites = { "verb.shutdown", "object.kmap" }
                                                         , .description = "Ensures deltas are saved if user desires, and if exiting the program is really desired."
                                                         , .action = outlet_script } ) );

        auto const script = 
            fmt::format(
                R"%%%( 
                const requisites = to_VectorString( {} );
                window.onbeforeunload = function(e){{ kmap.event_store().fire_event( requisites ); e.returnValue = false; return false; }};
                )%%%"
                , "[ 'subject.kmap', 'verb.shutdown' ]" );
        KMAP_TRY( js::eval_void( script ) );
    #endif // 0
    }

    rv = outcome::success();
    
    return rv;
}

auto Kmap::clear()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    // TODO: Rather than maintaining a list of reset_*s, why not place all of these in Environment, in their dependent order, and thereby just reset env,
    //       and get the reset for free of all these?
    KTRY( clear_component_store() );

    rv = outcome::success();

    return rv;
}

auto Kmap::clear_component_store()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( component_store_->clear() );

    component_store_ = nullptr;

    rv = outcome::success();

    return rv;
}

auto Kmap::on_leaving_editor()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const nw = KTRY( fetch_component< com::Network >() );
    auto const tv = KTRY( fetch_component< com::TextArea >() );
    auto const contents = tv->editor_contents();
    auto const rid = nw->resolve( nw->selected_node() );
    auto const canvas = KTRY( fetch_component< com::Canvas >() );
    auto const db = KTRY( fetch_component< com::Database >() );

    KMAP_ENSURE( nw->exists( rid ), error_code::network::invalid_node );

    KTRY( db->update_body( rid, contents ) );
    KTRY( canvas->hide( canvas->editor_pane() ) );
    KTRY( nw->select_node( nw->selected_node() ) ); // Ensure the newly added preview is updated.

    rv = outcome::success();

    return rv;
}

auto Kmap::load( FsPath const& db_path
               , std::set< std::string > const& components )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "db_path", db_path.string() );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const full_path = com::kmap_root_dir / db_path;

    KMAP_ENSURE_MSG( fs::exists( full_path ), error_code::common::uncategorized, io::format( "unable to find file {}", full_path.string() ) );

    // 1. Clear existing state. This *should* - if components are implemented correctly - mean that all events, html elements, etc. are destroyed.
    KTRY( clear() );
    init_component_store();
    KTRY( register_components( components ) );

    database_path_ = full_path; // Set state, so Database::load can read.

    KTRY( component_store_->fire_loaded( "component_store" ) );
    
    if( auto const estore = fetch_component< com::EventStore >()
      ; estore )
    {
        KTRY( estore.value()->fire_event( { "verb.loaded", "object.kmap" } ) );
    }

    rv = outcome::success();

    return rv;
}

auto Kmap::load( FsPath const& db_path )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "db_path", db_path.string() );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const all_components = KTRY( fetch_listed_components() );

    KTRY( load( db_path, all_components ) );

    rv = outcome::success();

    return rv;
}

Kmap& Singleton::instance()
{
    if( !inst_ )
    {
        try
        {
            inst_ = std::make_unique< Kmap >();
            if( !inst_ )
            {
                io::print( "failed to intitialize Kmap instance\n" );
            }
        }
        catch( std::exception& e )
        {
            io::print( "Singleton::instance exception: {}\n", e.what() );
        }
    }

    return *inst_;
}

std::unique_ptr< Kmap > Singleton::inst_ = {};

} // namespace kmap

