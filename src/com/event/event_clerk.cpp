/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/event/event_clerk.hpp>

#include <cmd/parser.hpp>
#include <com/event/event.hpp>
#include <contract.hpp>
#include <error/master.hpp>
#include <js_iface.hpp>
#include <kmap.hpp>
#include <path.hpp>
#include <path/act/fetch_body.hpp>
#include <path/node_view.hpp>
#include <path/node_view2.hpp>
#include <test/util.hpp>
#include <util/clerk/clerk.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>

namespace rvs = ranges::views;

namespace kmap::com {

EventClerk::EventClerk( Kmap& km )
    : kmap{ km }
{
}

EventClerk::EventClerk( Kmap& km
                      , std::set< std::string > const& outlet_com_reqs )
    : EventClerk{ km }
{
    outlet_com_requisites = outlet_com_reqs
                          | ranges::views::transform( []( auto const& c ){ return fmt::format( "component.{}", c ); } )
                          | ranges::to< decltype( outlet_com_requisites ) >();
}

EventClerk::~EventClerk()
{
    try
    {
        // So, problem here is... a component (e.g., LogStore) can use EventStore as a member, and never get initialized before it's destructed.
        // This means there'd be no guarantee that its requisites are initialized, meaning getting EventStore is unreliable, hence the throw.
        // Not sure I like it, but I can safely say that there's nothing to uninstall if EventStore can't be found.
        if( auto const estorec = kmap.fetch_component< com::EventStore >()
          ; estorec )
        {
            auto& estore = estorec.value();
            auto const handle_result = []( auto const& res )
            {
                if( !res
                 && res.error().ec != error_code::network::invalid_node ) // invalid_node => !exists - fine - no need to erase already erased.
                {
                    KMAP_THROW_EXCEPTION_MSG( kmap::error_code::to_string( res.error() ) ); \
                }
            };

            for( auto const& e : outlet_transitions | ranges::views::reverse ) { handle_result( estore->uninstall_outlet( e ) ); }
            for( auto const& e : outlets | ranges::views::reverse ) { handle_result( estore->uninstall_outlet( e ) ); }
            for( auto const& e : objects | ranges::views::reverse ) { handle_result( estore->uninstall_object( e ) ); }
            for( auto const& e : components | ranges::views::reverse ) { handle_result( estore->uninstall_component( e ) ); }
            for( auto const& e : verbs | ranges::views::reverse ) { handle_result( estore->uninstall_verb( e ) ); }
            for( auto const& e : subjects | ranges::views::reverse ) { handle_result( estore->uninstall_subject( e ) ); }
        }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto EventClerk::append_com_reqs( std::set< std::string > const& requisites )
    -> std::set< std::string >
{
    auto nreqs = requisites;

    nreqs.insert( outlet_com_requisites.begin(), outlet_com_requisites.end() );

    return nreqs;
}

auto EventClerk::append_com_reqs( Leaf const& leaf )
    -> Leaf
{
    auto nleaf = leaf;

    nleaf.requisites = append_com_reqs( nleaf.requisites );

    return nleaf;
}

auto EventClerk::append_com_reqs( Branch const& branch )
    -> Branch
{
    auto nb = branch;

    nb.requisites = append_com_reqs( nb.requisites );

    return nb;
}

auto EventClerk::check_registered( Leaf const& leaf )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "leaf", leaf.heading );
    
    auto rv = KMAP_MAKE_RESULT( void );

    if( auto const lnode = view2::event::event_root
                         | view2::child( "outlet" )
                         | view2::direct_desc( leaf.heading )
      ; lnode | act2::exists( kmap ) )
    {
        auto const eroot = KTRY( view2::event::event_root | act2::fetch_node( kmap ) );
        auto const node = KTRY( lnode | act2::fetch_node( kmap ) );
        auto const matches = util::match_body_code( kmap, lnode | view2::child( "action" ), leaf.action )
                          && match_requisites( kmap, eroot, node, leaf.requisites )
                          && util::match_raw_body( kmap, lnode | view2::child( "description" ), leaf.description )
                           ;

        if( !matches )
        {
            if( util::confirm_reinstall( "Outlet", leaf.heading ) )
            {
                KTRY( lnode | act2::erase_node( kmap ) );
                KTRY( install_outlet( leaf ) ); // Re-install outlet.
            }
        }
    }
    else
    {
        if( util::confirm_reinstall( "Outlet", leaf.heading ) )
        {
            KTRY( install_outlet( leaf ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::check_registered( Branch const& branch )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "branch", branch.heading );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const eroot = KTRY( estore->event_root() );

    if( auto const bnode = anchor::node( eroot )
                         | view2::child( "outlet" )
                         | view2::direct_desc( branch.heading )
                         | act2::fetch_node( kmap )
      ; bnode )
    {
        auto const matches = match_requisites( kmap, eroot, bnode.value(), branch.requisites );

        if( !matches )
        {
            if( KTRY( util::confirm_reinstall( "Outlet", branch.heading ) ) )
            {
                KTRY( view::make( bnode.value() )
                    | view::erase_node( kmap ) );
                KTRY( install_outlet( branch ) );
            }
        }
        else
        {
            auto const dispatch = util::Dispatch
            {
                [ & ]( Branch const& arg )
                {
                    auto tbranch = arg;
                    tbranch.heading = fmt::format( "{}.{}", branch.heading, arg.heading );
                    return check_registered( tbranch );
                }
            ,   [ & ]( Leaf const& arg )
                {
                    auto tleaf = arg;
                    tleaf.heading = fmt::format( "{}.{}", branch.heading, arg.heading );
                    return check_registered( tleaf );
                }
            };

            for( auto const& transition : branch.transitions )
            {
                KTRY( std::visit( dispatch, transition ) );
            }
        }
    }
    else
    {
        if( KTRY( util::confirm_reinstall( "Outlet", branch.heading ) ) )
        {
            KTRY( install_outlet( branch ) );
        }
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::check_registered( std::string const& heading )
    -> Result< void >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( registered_outlets.contains( heading ), error_code::network::invalid_heading );

    auto const r_entry = registered_outlets.at( heading );
    auto const dispatch = util::Dispatch
    {
        [ & ]( Leaf const& arg ){ return check_registered( arg ); }
    ,   [ & ]( Branch const& arg ){ return check_registered( arg ); }
    };

    KTRY( std::visit( dispatch, r_entry ) );

    rv = outcome::success();

    return rv;
}

auto EventClerk::check_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& heading : registered_outlets
                             | ranges::views::keys )
    {
        KTRY( check_registered( heading ) );
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::fire_event( std::set< std::string > const& requisites )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );

    KTRY( install_requisites( requisites ) );

    if( payload )
    {
        KTRY( estore->fire_event( requisites, payload.value() ) );
    }
    else
    {
        KTRY( estore->fire_event( requisites ) );
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::fire_event( std::set< std::string > const& requisites
                           , EventStore::Payload const& event_payload )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );

    payload = event_payload;
    {
        KTRY( fire_event( requisites ) );
    }
    payload = std::nullopt; // TODO: Still need to null payload even if fire_event fails.

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_subject( Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const subject = KTRY( estore->install_subject( heading ) );

    subjects.emplace_back( subject );

    rv = subject;

    return rv;
}

auto EventClerk::install_verb( Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const verb = KTRY( estore->install_verb( heading ) );

    verbs.emplace_back( verb );

    rv = verb;

    return rv;
}

auto EventClerk::install_object( Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const object = KTRY( estore->install_object( heading ) );

    objects.emplace_back( object );

    rv = object;

    return rv;
}

auto EventClerk::install_component( Heading const& heading )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "heading", heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const com = KTRY( estore->install_component( heading ) );

    components.emplace_back( com );

    rv = com;

    return rv;
}

auto EventClerk::install_outlet( Leaf const& leaf ) 
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "leaf", leaf.heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const amended_leaf = append_com_reqs( leaf );

    KTRY( install_requisites( amended_leaf.requisites ) );

    auto const outlet = KTRY( estore->install_outlet( amended_leaf ) );

    outlets.emplace_back( outlet );

    rv = outlet;

    return rv;
}

auto EventClerk::install_outlet( Branch const& branch ) 
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "branch", branch.heading );

    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const amended_branch = append_com_reqs( branch );

    KTRY( install_requisites( gather_requisites( amended_branch ) ) );

    auto const outlet = KTRY( estore->install_outlet( amended_branch ) );

    outlets.emplace_back( outlet );

    rv = outlet;

    return rv;
}

auto EventClerk::install_outlet( std::string const& path )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_STR( "path", path );

    auto rv = KMAP_MAKE_RESULT( Uuid );

    KMAP_ENSURE( registered_outlets.contains( path ), error_code::network::invalid_heading );

    auto const r_entry = registered_outlets.at( path );
    auto const dispatch = util::Dispatch
    {
        [ & ]( Leaf const& arg ){ return install_outlet( arg ); }
    ,   [ & ]( Branch const& arg ){ return install_outlet( arg ); }
    };

    rv = KTRY( std::visit( dispatch, r_entry ) );

    return rv;
}

auto EventClerk::install_outlet_transition( Uuid const& root
                                          , Transition const& transition )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    KMAP_THROW_EXCEPTION_MSG( "TODO: Need to install_requisites() first" );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const ot = KTRY( estore->install_outlet_transition( root, transition ) );

    outlet_transitions.emplace_back( ot );

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_requisites( std::set< std::string > const& requisites )
    -> Result< void >
{
    KM_RESULT_PROLOG();

    auto rv = KMAP_MAKE_RESULT( void );
    auto const estore = KTRY( kmap.fetch_component< com::EventStore >() );
    auto const eroot = KTRY( estore->event_root() );

    for( auto const& req : requisites )
    {
        if( !( view::make( eroot )
             | view::direct_desc( req )
             | view::exists( kmap ) ) )
        {
            // TODO: Fix this...
            auto const ps = req
                          | ranges::views::split( '.' )
                          | ranges::views::drop( 1 )
                          | ranges::views::join( '.' )
                          | ranges::to< std::string >();

            if( req.starts_with( "subject" ) )
            {
                KTRY( install_subject( ps ) );
            }
            else if( req.starts_with( "verb" ) )
            {
                KTRY( install_verb( ps ) );
            }
            else if( req.starts_with( "object" ) )
            {
                KTRY( install_object( ps ) );
            }
            else if( req.starts_with( "component" ) )
            {
                KTRY( install_component( ps ) );
            }
            else
            {
                KMAP_THROW_EXCEPTION_MSG( "unexpected event requisite category" );
            }
        }
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::install_registered()
    -> Result< void >
{
    KM_RESULT_PROLOG();
    
    auto rv = KMAP_MAKE_RESULT( void );

    for( auto const& path : registered_outlets
                          | ranges::views::keys )
    {
        KTRY( install_outlet( path ) );
    }

    rv = outcome::success();

    return rv;
}

auto EventClerk::register_outlet( com::Transition const& t ) 
    -> void
{
    std::visit( [ & ]( auto const& v )
                { 
                    KMAP_ENSURE_EXCEPT( !registered_outlets.contains( v.heading ) );

                    auto const tt = append_com_reqs( v );

                    registered_outlets.emplace( v.heading, tt );
                }
              , t );
}

SCENARIO( "EventClerk::register_outlet", "[event][clerk]" )
{
    // TODO: Ensure duplicate insertions are caught;
    //       Ensure preset component requisites are inserted automatically.
}

auto gather_requisites( Transition const& t )
    -> std::set< std::string >
{
    auto rv = std::set< std::string >{};
    auto const dispatch = util::Dispatch
    {
        []( Leaf const& x )
        {
            return x.requisites;
        }
    ,   []( Branch const& x )
        {
            auto rs = x.requisites;
            for( auto const& tt : x.transitions )
            {
                auto const grs = gather_requisites( tt );
                rs.insert( grs.begin(), grs.end() );
            }
            return rs;
        }
    };

    return std::visit( dispatch, t );
}

auto match_requisites( Kmap const& km
                     , Uuid const eroot
                     , Uuid const& lnode
                     , std::set< std::string > const& alias_paths )
    -> bool
{
    auto rv = false;
    auto const rreq_srcs = view::make( eroot )
                         | view::direct_desc( view::all_of( alias_paths ) )
                         | view::to_node_set( km )
                         | ranges::to< std::vector >();
    auto const oreq_srcs = view::make( lnode )
                         | view::child( "requisite" )
                         | view::alias
                         | view::resolve
                         | view::to_node_set( km )
                         | ranges::to< std::vector >();

    rv = ( ranges::distance( rvs::set_intersection( rreq_srcs, oreq_srcs ) ) == rreq_srcs.size() )
      && ( rreq_srcs.size() == alias_paths.size() );

    return rv;
}

} // namespace kmap::com
