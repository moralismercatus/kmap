/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "component_store.hpp"

#include "component.hpp"
#include "contract.hpp"
#include "kmap.hpp"

#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>

#include <set>

namespace rvs = ranges::views;

namespace kmap {

ComponentStore::ComponentStore( Kmap& km )
    : km_{ km }
{
}

// Components must be removed according to their dependencies, such that any dependent cleanup can be done.
// TODO: Unit test that components are removed in proper order.
ComponentStore::~ComponentStore()
{
    // Hmm... identify components that have a single requisite. I think this is necessary, if I hold to the position that
    // all requisites are fired from a chain that starts from a root...
    try
    {
        if( !initialized_components_.empty() )
        {
            KMAP_THROW_EXCEPTION_MSG( "clear() must be called prior to destruction, as components may depend on ComponentStore" );
        }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto ComponentStore::all_initialized_components()
    -> std::set< std::string >
{
    auto rv = std::set< std::string >{};

    rv = initialized_components_
       | rvs::keys
       | ranges::to< std::set >();

    return rv;
}

auto ComponentStore::all_uninit_dependents( std::string const& component )
    -> std::set< std::string >
{
    auto rv = std::set< std::string >{};
    auto const& uc = uninitialized_components_;

    if( uc.contains( component ) )
    {
        auto const com = uc.at( component );
        auto const reqs = com->requisites();

        for( auto const& req : reqs )
        {
            auto const deps = all_uninit_dependents( req );

            rv.insert( deps.begin(), deps.end() );
        }

        rv.insert( reqs.begin(), reqs.end() );
    }

    return rv;
}

// TODO: Make better...
// This is, in fact, a hack, for Kmap to call before resetting its pointer to ComponentStore.
// Here's the problem:
// When Kmap resets `component_store_ = std::nullptr;`, `component_store_` becomes null while destructing.
// But as it is destructing, its components may attempt clean up with calls to Kmap::component_store(), which then dereferences the null `component_store_`,
// So, this is a workaround (read: hack) such that the components can be destructed while Kmap::component_store_ is valid.
auto ComponentStore::clear()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    // Erase components until none remain.
    while( !initialized_components_.empty() )
    {
        KTRY( erase_component( initialized_components_.begin()->second ) );
    }

    BC_ASSERT( initialized_components_.empty() );

    uninitialized_components_ = {};

    rv = outcome::success();

    return rv;
}

auto ComponentStore::erase_component( ComponentPtr const com )
    -> Result< void >
{
    BC_ASSERT( com );

    fmt::print( "[component] erasing component: {}\n", com->name() );

    auto rv = KMAP_MAKE_RESULT( void );
    auto const depends_on_com = [ & ]( auto const& e ){ return e.second->requisites().contains( std::string{ com->name() } ); };

    while( ranges::count_if( initialized_components_, depends_on_com ) > 0 )
    {
        KTRY( erase_component( ranges::find_if( initialized_components_, depends_on_com )->second ) );
    }

    initialized_components_.erase( std::string{ com->name() } );

    fmt::print( "[component] erased component: {}\n", com->name() );

    rv = outcome::success();

    return rv;
}

// Q: Can I install a component_store_outlet?
auto ComponentStore::install_standard_events()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    

    // TODO: Are events even being dispatched...? Does this matter at all?
    // TODO: I'm not even sure ComponentStore should be using an eclerk, given EventStore dependent on ComponentStore (even for its lifetime, possibly?).
    // KTRY( eclerk_.install_outlet( Leaf{ .heading = "component.register_initialization"
    //                                   , .requisites = { "subject.component", "verb.initialized" }
    //                                   , .description = "registers a component to the ComponentStore for dispatching dependent components"
    //                                   , .action = R"%%%(kmap.component_store().register_component( kmap.event_store().payload() );)%%%" } ) );

    rv = outcome::success();

    return rv;
}

auto ComponentStore::is_initialized( std::string const& id ) const
    -> bool
{
    return initialized_components_.contains( id );
}

// TODO: This needs to ensure that a component does not have a circular requisite set, either itself as a requisite, or that scenario transitively.
auto ComponentStore::register_component( ComponentCtorPtr const& cctor )
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( !initialized_components_.contains( cctor->name() ), error_code::common::uncategorized );

    if( !uninitialized_components_.contains( cctor->name() ) )
    {
        // fmt::print( "[component] register_component: {}\n", cctor->name() );

        uninitialized_components_.emplace( cctor->name(), cctor );

        rv = outcome::success();
    }

    return rv;
}

// TODO: Should we require that "id" maps to a registered component?
auto ComponentStore::fire_initialized( std::string const& id )
    -> Result< void >
{
    fmt::print( "[component] fire_initialized: {}\n", id );

    auto rv = KMAP_MAKE_RESULT( void );

    received_inits_.emplace( id );
    
    auto const component_meets_reqs = [ & ]( auto const& com )
    {
        return ranges::all_of( com.second->requisites()
                             , [ & ]( auto const& req ) { return received_inits_.contains( req ); } );
    };
    auto const uc_it = ranges::find_if( uninitialized_components_, component_meets_reqs );
    auto const uc_it_end = uninitialized_components_.end();

    if( uc_it != uc_it_end )
    {
        auto const [ name, cctor ] = *uc_it; // Note: No reference. Iterator become invalid.
        auto com = cctor->construct( km_ );

        fmt::print( "[component] initializing: {}\n", name );

        KTRY( com->initialize() );

        initialized_components_.emplace( name, com );

        if( uninitialized_components_.contains( name ) )
        {
            uninitialized_components_.erase( name );
        }

        KTRY( fire_initialized( name ) ); // Cascade.
    }

    rv = outcome::success();

    return rv;
}

// TODO: This is effectively identical to fire_initialized except for _loaded suffix. Unite them.
// Note: A component enters an initialized state, i.e., made an element of `initialized_components_` after load.
auto ComponentStore::fire_loaded( std::string const& id )
    -> Result< void >
{
    fmt::print( "[component] fire_loaded: {}\n", id );

    auto rv = KMAP_MAKE_RESULT( void );

    received_inits_.emplace( id );
    
    auto const component_meets_reqs = [ & ]( auto const& com )
    {
        return ranges::all_of( com.second->requisites()
                             , [ & ]( auto const& req ) { return received_inits_.contains( req ); } );
    };
    auto const uc_it = ranges::find_if( uninitialized_components_, component_meets_reqs );
    auto const uc_it_end = uninitialized_components_.end();

    if( uc_it != uc_it_end )
    {
        auto const [ name, cctor ] = *uc_it; // Note: No reference. Iterator become invalid.
        auto com = cctor->construct( km_ );

        fmt::print( "[component] loading: {}\n", name );

        KTRY( com->load() );

        initialized_components_.emplace( name, com );

        if( uninitialized_components_.contains( name ) )
        {
            uninitialized_components_.erase( name );
        }

        KTRY( fire_loaded( name ) ); // Cascade.
    }

    rv = outcome::success();

    return rv;
}

} // namespace kmap

namespace kmap::binding {

struct ComponentStore
{
    auto register_component( std::string const& id )
        -> void;
};

} // namespace kmap::binding

// EMSCRIPTEN_BINDINGS( kmap_component_store )
// {
// }
