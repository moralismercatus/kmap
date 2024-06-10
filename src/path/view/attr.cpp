/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/attr.hpp>

#include <com/database/db.hpp>
#include <com/network/network.hpp>
#include <kmap.hpp>
#include <test/util.hpp>

#include <catch2/catch_test_macros.hpp>

#include <compare>
#include <variant>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Attr::create( CreateContext const& ctx
                 , Uuid const& root ) const
    -> Result< UuidSet >
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH_NODE( "node", root );

    auto const db = KTRY( ctx.km.fetch_component< com::Database >() );

    return UuidSet{ KTRY( com::create_attr_node( *db, root ) ) };
}

auto Attr::fetch( FetchContext const& ctx
                , Uuid const& node ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();

    auto rv = FetchSet{};
    auto const nw = KTRY( ctx.km.fetch_component< com::Network >() );

    if( auto const attr = nw->fetch_attr_node( node )
      ; attr )
    {
        rv.emplace( LinkNode{ .id=attr.value() } );
    }

    return rv;
}

SCENARIO( "view::Attr::fetch", "[node_view][attr]" )
{
    KMAP_COMPONENT_FIXTURE_SCOPED( "root_node", "network" );

    // auto& km = Singleton::instance();
    // auto const nw = REQUIRE_TRY( km.fetch_component< com::Network >() );
    // auto const root = nw->root_node();
}

auto Attr::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Attr >();
}

auto Attr::to_string() const
    -> std::string
{
    // if( pred_ )
    // {
    //     auto const dispatch = util::Dispatch
    //     {
    //         [ & ]( auto const& arg ){ return fmt::format( "attr( '{}' )", arg ); }
    //     ,   [ & ]( Link::LinkPtr const& arg ){ BC_ASSERT( arg ); return fmt::format( "alias( {} )", ( *arg ) | act::to_string ); }
    //     };

    //     return std::visit( dispatch, pred_.value() );
    // }
    // else
    {
        return "attr";
    }
}

auto Attr::compare_less( Link const& other ) const
    -> bool
{ 
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return false; //pred_ < dynamic_cast< decltype( *this )& >( other ).pred_;
    }
    else if( cmp == std::strong_ordering::less )
    {
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace kmap::view2