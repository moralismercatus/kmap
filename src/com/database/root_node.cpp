/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "com/database/root_node.hpp"

#include "com/database/db.hpp"
#include "kmap.hpp"

namespace kmap::com {

auto RootNode::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    root_ = gen_uuid();

    KMAP_ENSURE( root_ != Uuid{ 0 }, error_code::common::uncategorized );

    auto const heading = "root";
    auto const title = format_title( heading );
    auto const db = KTRY( fetch_component< com::Database >() );

    KTRY( db->push_node( root_ ) );
    KTRY( db->push_heading( root_, heading ) );
    KTRY( db->push_title( root_, title ) );
    KTRY( db->push_body( root_, "Welcome.\n\nType `:help` for assistance." ) );

    auto const attrn = KTRY( create_attr_node( *db, root_ ) );

    // TODO: Abstract into protected fn shared amongst create_child and this.
    {
        auto const gn = gen_uuid();

        KTRY( db->push_node( gn ) );
        KTRY( db->push_heading( gn, "genesis" ) );
        KTRY( db->push_title( gn, "Genesis" ) );
        KTRY( db->push_child( attrn, gn ) );
        KTRY( db->push_body( gn, std::to_string( present_time() ) ) );
    }

    rv = outcome::success();

    return rv;
}

auto RootNode::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const db = KTRY( fetch_component< com::Database >() );
    auto const& nt = db->fetch< db::NodeTable >();

    KMAP_ENSURE( ranges::distance( nt ) >= 1, error_code::network::invalid_root );

    auto const fetch_parent = [ &db ]( auto const& node )
    {
        if( auto const cp = db->fetch_parent( node )
          ; cp )
        {
            return cp;
        }

        return db->fetch_attr_owner( node );
    };
    auto const start = nt.begin()->key();
    auto parent = fetch_parent( start );
    auto child = decltype( parent ){ start };

    while( parent.has_value() )
    {
        child = parent;
        parent = fetch_parent( parent.value() );
    }

    root_ = child.value();

    rv = outcome::success();

    return rv;
}

auto RootNode::root_node() const
    -> Uuid const&
{
    if( root_ == Uuid{ 0 } )
    {
        KMAP_THROW_EXCEPTION_MSG( "root node uninitialized before access" );
    }

    return root_;
}

namespace
{
    namespace root_node_def
    {
        using namespace std::string_literals;

        REGISTER_COMPONENT
        (
            kmap::com::RootNode
        ,   std::set({ "database"s })
        ,   "establishes root node and adds it to database"
        );
    } // namespace timer_def 
} // namespace anonymous

} // namespace kmap::com
