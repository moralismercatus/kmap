/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "kmap.hpp"

#include "canvas.hpp"
#include "cli.hpp"
#include "cmd.hpp"
#include "common.hpp"
#include "contract.hpp"
#include "db.hpp"
#include "db.hpp"
#include "environment.hpp"
#include "error/master.hpp"
#include "error/network.hpp"
#include "error/node_manip.hpp"
#include "io.hpp"
#include "js_iface.hpp"
#include "lineage.hpp"
#include "network.hpp"
#include "path.hpp"

#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <range/v3/action/join.hpp>
#include <range/v3/action/reverse.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/replace.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/take.hpp>
#include <sqlpp11/insert.h>

#include <iostream>
#include <set>
#include <vector>

using namespace ranges;
namespace fs = boost::filesystem;

namespace kmap
{

Kmap::Kmap()
    : env_{ std::make_unique< Environment >() }
    , canvas_{ std::make_unique< Canvas >( *this ) }
{
    io::print( "in kmap ctor\n" );
    // if( auto const r = reset()
    //   ; !r )
    // {
    //     KMAP_THROW_EXCEPTION_MSG( to_string( r.error() ) );
    // }

    // for( auto const& c : cmd::make_core_commands( *this ) )
    // {
    //     cli_->register_command( c );
    // }
}

auto Kmap::set_up_db_root()
    -> void
{
    auto const id = root_node_id();
    auto const heading = "root";
    auto const title = format_title( heading );
    auto& db = database();
    auto nt = nodes::nodes{};
    auto ht = headings::headings{};
    auto tt = titles::titles{};
    auto bt = bodies::bodies{};

    db.execute( insert_into( nt )
              . set( nt.uuid = to_string( id ) ) );
    db.execute( insert_into( ht )
              . set( ht.uuid = to_string( id )
                   , ht.heading = heading ) );
    db.execute( insert_into( tt )
              . set( tt.uuid = to_string( id )
                   , tt.title = title ) );
    db.execute( insert_into( bt )
              . set( bt.uuid = to_string( id )
                   , bt.body =  "Welcome.\n\nType `:help` for assistance." ) );
}

auto Kmap::set_up_nw_root()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    auto const id = root_node_id();
    auto const heading = fetch_heading( id ).value();
    auto const title = fetch_title( id ).value();
    auto& nw = network();

    nw.create_node( id, title );
    ( void )nw.select_node( id );

    rv = outcome::success();

    return rv;
}

auto Kmap::set_ordering_position( Uuid const& id
                                , uint32_t pos )
    -> bool
{
    auto rv = false;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( fetch_siblings_inclusive( id ).size() > pos );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( fetch_ordering_position( id ) == pos );
            }
        })
    ;
    
    if( auto siblings = fetch_siblings_inclusive_ordered( id )
      ; siblings.size() > pos )
    {
        auto const it = find( siblings, id );

        BC_ASSERT( it != end( siblings) );

        std::iter_swap( it
                      , begin( siblings ) + pos );

        reorder_children( fetch_parent( id ).value()
                        , siblings );
    }

    return rv;
}

auto Kmap::update_body( Uuid const& id
                      , std::string_view const contents )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( resolve( id ) ) );
        })
    ;

    using sqlpp::update;

    // TODO: Refactor all instances of this code as a convenience function in Database.
    auto& db = database();
    auto bt = bodies::bodies{};

    db.execute( update( bt )
              . set( bt.body = contents )
              . where( bt.uuid == to_string( resolve( id ) ) ) );
}

auto Kmap::load_state( FsPath const& db_path )
    -> bool 
{
    auto const full_path = kmap_root_dir / db_path;
    if( !fs::exists( full_path ) )
    {
        return false;
    }

    // TODO: Precondition: db_path does not start with kmap_root_dir, as that's appended here.
    env_ = std::make_unique< Environment >( full_path );

    auto const& db = database();

    {
        auto ct = children::children{};
        auto rr = db.execute( select( all_of( ct ) )
                                    . from( ct )
                                    . where( ct.parent_uuid.not_in( select( ct.child_uuid )
                                                                  . from( ct )
                                                                  . unconditionally() ) ) );

        // BC_ASSERT( 1 == std::distance( rr.begin(), rr.end() ) );

        auto const root_id = uuid_from_string( rr.front().parent_uuid ).value();

        env_->set_root( root_id );

        {
            auto& nw = network();

            nw.create_node( root_id
                          , fetch_title( root_id ).value() );
            nw.select_node( root_id );
        }

        select_node( root_id );
        // TODO: post_condition must require root_node_id_ to be valid.
    }
    {
        auto at = aliases::aliases{};

        for( auto const& c : db.execute( select( all_of( at ) )
                                       . from( at )
                                       . unconditionally() ) )
        {
            create_alias_internal( uuid_from_string( c.src_uuid ).value()
                                 , uuid_from_string( c.dst_uuid ).value() );
        }
        for( auto const& c : db.execute( select( all_of( at ) )
                                       . from( at )
                                       . unconditionally() ) )
        {
            update_aliases( uuid_from_string( c.dst_uuid ).value() );
        }
    }

    return true;
}

auto Kmap::travel_left()
    -> Result< Uuid >
{
    auto const selected = selected_node();
    auto const parent = KMAP_TRY( fetch_parent( selected ) );

    KMAP_TRY( select_node( parent ) );

    return parent;
}

auto Kmap::travel_right()
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const selected = selected_node();
    auto const children = fetch_children_ordered( selected );

    if( !children.empty() )
    {
        auto const dst = mid( children );

        KMAP_TRY( select_node( dst ) );
        rv = dst;
    }

    return rv;
}

auto Kmap::travel_up()
    -> Result< Uuid >
{
    auto const selected = selected_node();
    auto const above = KMAP_TRY( fetch_above( selected ) );

    KMAP_TRY( select_node( above ) );

    return above;
}

auto Kmap::travel_down()
    -> Result< Uuid >
{
    auto const selected = selected_node();
    auto const below = KMAP_TRY( fetch_below( selected ) );

    KMAP_TRY( select_node( below ) );

    return below;
}

auto Kmap::travel_bottom()
    -> Uuid
{
    auto const selected = selected_node();
    auto rv = selected;

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        auto const children = fetch_children_ordered( parent.value() );

        select_node( children.back() );

        rv = children.back();
    }

    return rv;
}

auto Kmap::travel_top()
    -> Uuid
{
    auto const selected = selected_node();
    auto rv = selected;

    if( auto const parent = fetch_parent( selected )
      ; parent )
    {
        auto const children = fetch_children_ordered( parent.value() );

        select_node( children.front() );

        rv = children.front();
    }

    return rv;
}

auto Kmap::instance()
    -> Environment&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            assert( env_ );
        })
    ;

    return *env_;
}

auto Kmap::instance() const
    -> Environment const&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            assert( env_ );
        })
    ;

    return *env_;
}

auto Kmap::database()
    -> Database&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            assert( env_ );
        })
    ;

    return env_->database();
}

auto Kmap::database() const
    -> Database const&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            assert( env_ );
        })
    ;

    return env_->database();
}

auto Kmap::network()
    -> Network&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            assert( canvas_ );
        })
    ;

    KMAP_LOG_LINE();

    if( !network_ )
    {
        network_ = std::make_unique< Network >( canvas_->network_pane() );
    }

    KMAP_LOG_LINE();

    return *network_;
}

auto Kmap::network() const
    -> Network const&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            assert( canvas_ );
            assert( network_ );
        })
    ;

    return *network_;
}

auto Kmap::aliases()
    -> Aliases&
{
    return aliases_;
}

auto Kmap::aliases() const
    -> Aliases const&
{
    return aliases_;
}

auto Kmap::move_body( Uuid const& src
                    , Uuid const& dst )
    -> bool
{
    return kmap::move_body( *this
                          , src
                          , dst );
}

auto Kmap::move_state( FsPath const& dst )
    -> bool
{
    using boost::system::error_code;

    if( !file_exists( dst ) )
    {
        return false;
    }

    auto ec = error_code{};

    fs::rename( database().path()
              , kmap_root_dir / dst
              , ec );

    if( ec )
    {
        return false;
    }

    return load_state( dst );
}

auto Kmap::copy_body( Uuid const& src
                    , Uuid const& dst )
    -> bool
{
    return kmap::copy_body( *this
                          , src
                          , dst );
}

auto Kmap::copy_state( FsPath const& dst )
    -> bool
{
    using boost::system::error_code;

    if( file_exists( dst ) )
    {
        return false;
    }

    auto ec = error_code{};

    fs::copy_file( database().path()
                 , kmap_root_dir / dst
                 , ec );

    return !ec;
}

auto Kmap::cli()
    -> Cli&
{
    if( !cli_ )
    {
        cli_ = std::make_unique< Cli >( *this );
    }

    return *cli_;
}

auto Kmap::cli() const
    -> Cli const&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( cli_ );
        })
    ;

    return *cli_;
}

auto Kmap::canvas()
    -> Canvas&
{
    return *canvas_;
}

auto Kmap::canvas() const
    -> Canvas const&
{
    return *canvas_;
}

auto Kmap::text_area()
    -> TextArea&
{
    if( !text_area_ )
    {
        text_area_ = std::make_unique< TextArea >( *this );
    }

    return *text_area_;
}

auto Kmap::text_area() const
    -> TextArea const&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( text_area_ );
        })
    ;

    return *text_area_;
}

auto Kmap::root_node_id() const
    -> Uuid const&
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( env_ );
        })
    ;

    return env_->root_node_id();
}

auto Kmap::parse_cli( std::string const& input )
    -> void
{
    if( !cli().parse_raw( input ) )
    {
        network().focus();
    }
}

auto Kmap::absolute_path_uuid( Lineal const& node ) const
    -> UuidPath
{
    auto rv = UuidPath{};
    auto const [ root, target ] = node;

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( exists( root, target ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
        BC_POST([ & ]
        {
            // BC_ASSERT( rv.at( 0 ) == root ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    auto current = target;

    while( current != root )
    {
        rv.emplace_back( current );

        current = fetch_parent( current ).value();
    }

    rv.emplace_back( current ); // Root.

    rv |= actions::reverse;

    return rv;
}

auto Kmap::absolute_path_uuid( Uuid const& node ) const
    -> UuidPath
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( exists( root, target ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    auto const lineal = make< Lineal >( *this, root_node_id(), node ).value();

    return absolute_path_uuid( lineal );
}

auto Kmap::fetch_ancestral_lineage( Uuid const& root
                                  , Uuid const& id
                                  , uint32_t const& max ) const
    -> UuidPath
{
    auto rv = UuidPath{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
            BC_ASSERT( is_lineal( root, id ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( is_descending( *this, rv ) );
        })
    ;

    auto current = id;
    auto count = uint32_t{};

    while( current != root
        && count < max )
    {
        rv.emplace_back( current );

        current = fetch_parent( current ).value();

        ++count;
    }

    if( current == root )
    {
        rv.emplace_back( current );
    }

    rv |= actions::reverse;

    return rv;
}

auto Kmap::fetch_ancestral_lineage( Uuid const& id
                                  , uint32_t const& max ) const
    -> UuidPath
{
    return fetch_ancestral_lineage( root_node_id()
                                  , id
                                  , max );
}

// TODO: Reimpl. in terms of absolute_path_uuid.
auto Kmap::absolute_path( Uuid const& root
                        , Uuid const& id ) const
    -> HeadingPath
{
    auto rv = HeadingPath{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    auto current = id;

    while( current != root )
    {
        BC_ASSERT( current != root_node_id() );

        rv.emplace_back( fetch_heading( current ).value() );

        current = fetch_parent( current ).value();
    }

    rv.emplace_back( fetch_heading( current ).value() ); // Root heading.

    rv |= actions::reverse;

    return rv;
}

// TODO: Reimpl. in terms of absolute_path_uuid.
auto Kmap::absolute_path( Uuid const& id ) const
    -> HeadingPath
{
    return absolute_path( root_node_id()
                        , id );
}

auto Kmap::absolute_ipath( Uuid const& id ) const
    -> HeadingPath
{
    auto rv = HeadingPath{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    rv = absolute_path( id );
    rv |= actions::reverse;

    return rv;
}

auto Kmap::absolute_path_flat( Uuid const& id ) const
    -> Heading
{
    auto const ap = absolute_path( id );

    return '/' + ( ap
                 | views::drop( 1 ) // Dropping "root" b/c '/' is a unique synonym for it. 
                 | views::join( '.' )
                 | to< Heading >() );
}

auto Kmap::absolute_ipath_flat( Uuid const& id ) const
    -> InvertedHeading
{
    auto const ap = absolute_ipath( id );

    return ( ap
           | views::join( '.' )
           | to< Heading >() ) + '.';
}

// TODO: Ensure root exists.
/**
 * For most path operations, selected is required to be lineal to root.
 * When the root is specified - and not the true root - selected may be nonlineal.
 * To work around this case, this routine returns root, or otherwise the original selected.
 **/
auto Kmap::adjust_selected( Uuid const& root ) const
    -> Uuid
{
    auto const selected = selected_node();

    if( is_lineal( root, selected ) )
    {
        return selected;
    }
    else
    {
        return root;
    }
}

auto Kmap::fetch_above( Uuid const& id ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( rv, exists( id ), error_code::network::invalid_node );
    
    if( auto const children = fetch_parent_children_ordered( id )
      ; !children.empty() )
    {
        auto const it = find( children, id );

        if( it != begin( children ) )
        {
            rv = *std::prev( it );
        }
    }

    return rv;
}

auto Kmap::fetch_below( Uuid const& id ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );
            }
        })
    ;

    KMAP_ENSURE( rv, exists( id ), error_code::network::invalid_node );
    
    if( auto const children = fetch_parent_children_ordered( id )
      ; !children.empty() )
    {
        auto const it = find( children, id );

        if( it != end( children ) // TODO: I don't think it's even possible for it == end() here.
            && std::next( it ) != end( children ) )
        {
            rv = *std::next( it );
        }
    }

    return rv;
}

// TODO: I'm not sure that this routine does what it purports. Well, depending on definitions.
//       If an "alias" is just the first "pointer" node, then this purportment is true. 
//       If it's a subsequent node from the "pointer"/top-level-alias, then it's false.
auto Kmap::fetch_aliases_to( Uuid const& src ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( is_alias( e ) );
            }
        })
    ;

    auto const aliases = database().fetch_alias_destinations( src );

    rv = aliases
       | views::transform( [ & ]( auto const& dst ){ return make_alias_id( src 
                                                                         , dst ); } )
       | to< std::vector< Uuid > >();

    return rv;
}

auto Kmap::create_child( Uuid const& parent
                       , Uuid const& child
                       , Heading const& heading
                       , Title const& title )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( !exists( rv.value() ) );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );
                BC_ASSERT( is_child( parent
                                   , heading ) );
            }
        })
    ;

    KMAP_ENSURE( rv, is_valid_heading( heading ), error_code::node::invalid_heading );
    KMAP_ENSURE( rv, database().exists( resolve( parent ) ), error_code::create_node::invalid_parent );
    KMAP_ENSURE( rv, !exists( child ), error_code::create_node::node_already_exists );
    KMAP_ENSURE_MSG( rv, !is_child( parent, heading ), error_code::create_node::duplicate_child_heading, fmt::format( "{}:{}", absolute_path_flat( parent ), heading ) );

    // TODO: if either db insertion or network insertion fails, must undo the
    // the other that succeeds.
    auto& db = database();
    // auto const& child_path = parent_path / heading;
    auto const rpid = resolve( parent );

    // TODO: Must undo all if one fails.
    // Note: It's imperative that DB is added to first, as the alias check
    // relies on this fact when the network calls the update callback routine.
    {
        db.create_node( child
                      , rpid
                      , heading
                      , title );
    }

    rv = child;
    
    update_aliases( parent );

    return rv;
}

auto Kmap::create_child( Uuid const& parent
                       , Uuid const& child
                       , Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( is_valid_heading( heading ) ); // TODO: this function should return an outcome rather than fail with precondition.
            // BC_ASSERT( database().exists( resolve( parent ) ) ); // TODO: this function should return an outcome rather than fail with precondition.
            // BC_ASSERT( !exists( rv ) );
            // BC_ASSERT( !is_child( parent
            //                     , heading ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
        BC_POST([ & ]
        {
            // BC_ASSERT( exists( rv ) );
        })
    ;

    KMAP_ENSURE( rv, is_valid_heading( heading ), error_code::node::invalid_heading );

    rv = create_child( parent
                     , child
                     , heading
                     , format_title( heading ) );

    return rv;
}

auto Kmap::create_child( Uuid const& parent 
                       , Heading const& heading )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( is_valid_heading( heading ) ); // TODO: this function should return an outcome rather than fail with precondition.
            // BC_ASSERT( exists( resolve( parent ) ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
        BC_POST([ & ]
        {
            // BC_ASSERT( exists( rv ) );
        })
    ;

    rv = create_child( parent
                     , gen_uuid()
                     , heading );

    return rv;
}

auto Kmap::create_child( Uuid const& parent 
                       , Heading const& heading
                       , Title const& title )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // BC_ASSERT( is_valid_heading( heading ) ); // TODO: this function should return an outcome rather than fail with precondition.
            // BC_ASSERT( exists( resolve( parent ) ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
        BC_POST([ & ]
        {
            // BC_ASSERT( exists( rv ) );
        })
    ;

    rv = create_child( parent
                     , gen_uuid()
                     , heading
                     , title );

    return rv;
}

auto Kmap::create_descendants( Heading const& lineage )
    -> Result< std::vector< Uuid > >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    return kmap::create_descendants( *this
                                   , root_node_id()
                                   , adjust_selected( root_node_id() )
                                   , lineage );
}

auto Kmap::create_descendants( Uuid const& root
                             , Heading const& lineage )
    -> Result< std::vector< Uuid > >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    return kmap::create_descendants( *this
                                   , root
                                   , adjust_selected( root )
                                   , lineage );
}

auto Kmap::create_descendants( Uuid const& root
                             , Uuid const& selected
                             , Heading const& lineage )
    -> Result< std::vector< Uuid > >
{
    BC_CONTRACT()
        BC_PRE( [ & ]
        {
            BC_ASSERT( exists( root ) );
        } )
    ;

    return kmap::create_descendants( *this
                                   , root
                                   , selected
                                   , lineage );
}

// TODO: This fails if alias source is "/" - add a TC for this.
auto Kmap::create_alias( Uuid const& src
                       , Uuid const& dst )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
        BC_POST([ & ]
        {
            auto const asize = aliases_.size();
            BC_ASSERT( asize == alias_children_.size()
                    && asize == alias_parents_.size() );
            
            if( rv )
            {
                BC_ASSERT( exists( rv.value() ) );
            }
        })
    ;

    auto const rsrc = resolve( src );
    auto const rdst = resolve( dst );
    auto const alias_id = make_alias_id( rsrc
                                       , rdst );

    KMAP_ENSURE( rv, exists( rsrc ), error_code::create_alias::src_not_found );
    KMAP_ENSURE( rv, exists( rdst ), error_code::create_alias::dst_not_found );
    KMAP_ENSURE( rv, rsrc != rdst, error_code::create_alias::src_equals_dst );
    KMAP_ENSURE( rv, !is_lineal( rsrc, rdst ), error_code::create_alias::src_ancestor_of_dst );
    KMAP_ENSURE( rv, !exists( alias_id ), error_code::create_alias::alias_already_exists );
    KMAP_ENSURE( rv, !is_child( rdst, fetch_heading( rsrc ).value() ), error_code::create_node::duplicate_child_heading );

    auto& db = database();

    db.create_alias( rsrc
                   , rdst );
    aliases_.emplace( alias_id );
    alias_parents_.emplace( alias_id
                          , rdst );
    alias_children_.emplace( rdst
                           , alias_id );

    for( auto const& e : fetch_children( rsrc ) )
    {
        create_alias_internal( e
                             , alias_id );
    }

    update_aliases( rdst );

    rv = alias_id;

    return { rv };
}

auto Kmap::create_alias_internal( Uuid const& src
                                , Uuid const& dst )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( resolve( src ) ) ); // TODO: this function should return an outcome rather than fail with precondition.
            BC_ASSERT( exists( dst ) ); // TODO: this function should return an outcome rather than fail with precondition.
            BC_ASSERT( alias_children_.size() == alias_parents_.size() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( alias_children_.size() == alias_parents_.size() );
        })
    ;

    auto const rsrc = resolve( src );
    auto const alias_id = make_alias_id( rsrc
                                       , dst );

    aliases_.emplace( alias_id );
    alias_parents_.emplace( alias_id
                          , dst );
    alias_children_.emplace( dst
                           , alias_id );

    for( auto const& e : fetch_children( rsrc ) )
    {
        create_alias_internal( e
                             , alias_id );
    }
}

// TODO: This should make certain pre and post conditions (and return failure & undo if not met)
// about the coherency of the data being input in relation to assumptions by kmap.
auto Kmap::create_nodes( std::vector< std::pair< Uuid, std::string > > const& nodes
                       , std::vector< std::pair< Uuid, Uuid > > const& edges )
    -> void // TODO: Should return bool to indicate succ/fail.
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    auto& db = database();
    auto const c_to_p_map = edges
                          | views::transform( [ & ]( auto const& e ){ return std::pair{ e.second, e.first }; } )
                          | to< std::map< Uuid, Uuid > >();
    auto depth_of = [ & ]( auto const& e )
    {
        auto count = 1u;
        auto it = c_to_p_map.find( e );

        while( it != c_to_p_map.end() )
        {
            ++count;
            it = c_to_p_map.find( it->second );
        }

        return count;
    };
    auto sorted = nodes;
    sorted |= actions::sort( [ & ]( auto const& lhs
                                 , auto const& rhs )
    {
        return depth_of( lhs.first ) < depth_of( rhs.first );
    } );
    auto const node_limit = 1000u; // Currently, too large of values cause the network to grind to a halt. No good solution as of yet, so simply limiting here.
    auto const ids = sorted 
                   | views::keys 
                   | views::take( node_limit ) 
                   | to< std::vector< Uuid > >();
    auto const titles = sorted 
                      | views::values 
                      | views::take( node_limit ) 
                      | to< std::vector< Title > >();
    {
        for( auto i = 0u
           ; i < ids.size()
           ; ++i )
        {
            auto const child = ids[ i ];
            auto const parent = [ & ]
            {
                auto const it = c_to_p_map.find( child );

                if( it == c_to_p_map.end() )
                {
                    fmt::print( "should never reach\n" );
                }

                BC_ASSERT( it != c_to_p_map.end() );

                return it->second;
            }();

            db.create_node( child
                          , parent
                          , format_heading( titles[ i ] )
                          , titles[ i ] );
        }
    }
}

// TODO: This is not efficiently implemented. The data is not streamed into the
// DB, but rather copied twice - once into a compressed buffer and then again
// into a uint8_t buffer for consumption by the DB.
auto Kmap::store_resource( Uuid const& parent
                         , Heading const& heading
                         , std::byte const* data
                         , size_t const& size )
    -> Result< Uuid >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( database().exists( resolve( parent ) ) ); // TODO: this function should return an outcome rather than fail with precondition.
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
        auto& db = database();
        auto orst = original_resource_sizes::original_resource_sizes{};
        auto rt = resources::resources{};

        db.execute( insert_into( orst )
                  . set( orst.uuid = shash
                       , orst.resource_size = size ) );
        db.execute( insert_into( rt )
                  . set( rt.uuid = shash
                       , rt.resource = std::vector< uint8_t >{ reinterpret_cast< uint8_t const* >( cr.data() )
                                                             , reinterpret_cast< uint8_t const* >( cr.data() + cr.size() ) } ) );
    }

    return hash;
}

auto Kmap::reset()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );
    env_ = std::make_unique< Environment >();

    // TODO: propage errors for following calls:
    KMAP_LOG_LINE();
    database().create_tables();
    KMAP_LOG_LINE();
    set_up_db_root();
    KMAP_LOG_LINE();
    KMAP_TRY( canvas().reset() );
    KMAP_LOG_LINE();
    KMAP_TRY( set_up_nw_root() );
    KMAP_LOG_LINE();
    KMAP_TRY( cli().reset_all_preregistered() );
    KMAP_LOG_LINE();

    {
        KMAP_LOG_LINE();
        KMAP_TRY( select_node( root_node_id() ) );
        KMAP_LOG_LINE();
        KMAP_TRY( canvas().hide( canvas().editor_pane() ) );
        KMAP_LOG_LINE();
    }
    {
        KMAP_LOG_LINE();
        KMAP_TRY( js::eval_void( io::format( R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() ).onkeydown = function( e )
                                                   {{
                                                       let key = e.keyCode ? e.keyCode : e.which;
                                                       let is_ctrl = !!e.ctrlKey;
                                                       let is_shift = !!e.shiftKey;
                                                       let text = document.getElementById( kmap.uuid_to_string( kmap.canvas().cli_pane() ).value() );

                                                       const res = kmap.cli().on_key_down( key
                                                                                         , is_ctrl
                                                                                         , is_shift
                                                                                         , text.value );
 
                                                       if( key == 9/*tab*/
                                                        || key == 13/*enter*/ )
                                                       {{
                                                           e.preventDefault();
                                                       }}
                                                       if( res.has_error() )
                                                       {{
                                                           console.log( 'CLI error: ' + res.error_message() );
                                                       }}
                                                   }};)%%%" ) ) );
        KMAP_LOG_LINE();
        KMAP_TRY( js::eval_void( io::format( R"%%%(document.getElementById( kmap.uuid_to_string( kmap.canvas().editor_pane() ).value() ).onkeydown = function( e )
                                                   {{
                                                       let key = e.keyCode ? e.keyCode : e.which;
                                                       let is_ctrl = !!e.ctrlKey;
                                                       let is_shift = !!e.shiftKey;

                                                       if(   key == 27/*esc*/
                                                        || ( key == 67/*c*/ && is_ctrl ) )
                                                       {{
                                                           // Focus on network. There's already a callback to call 'on_leaving_editor' on focusout event.
                                                           kmap.canvas().focus( kmap.canvas().network_pane() );
                                                           e.preventDefault();
                                                       }}
                                                   }};)%%%" ) ) );
        KMAP_LOG_LINE();
    }

    rv = outcome::success();
        KMAP_LOG_LINE();
    
    return rv;
}

auto Kmap::rename( Uuid const& id
                 , Heading const& heading )
    -> void 
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( is_valid_heading( heading ) ); // TODO: this function should return an outcome rather than fail with precondition.
            BC_ASSERT( exists( id ) );
        })
        BC_POST([ & ]
        {
            // TODO: BC_ASSERT( ...db heading == heading );
        })
    ;

    {
        using sqlpp::update;

        auto& db = database();
        auto tt = headings::headings{};

        db.execute( update( tt )
                  . set( tt.heading = heading )
                  . where( tt.uuid == to_string( id ) ) );
    }
}

auto Kmap::exists( Uuid const& id ) const
    -> bool
{
    auto& db = database();

    return db.exists( resolve( id ) );
}

auto Kmap::exists( Heading const& heading ) const
    -> bool
{
    return fetch_leaf( root_node_id()
                     , selected_node()
                     , heading )
         . has_value();
}

// Note: 'children' should be unresolved. TODO: Actually, I suspect this is only a requirement for the precondition, which could always resolve all IDs. To confirm. I believe, when it comes to ordering, the resolved node is the only one used.
auto Kmap::reorder_children( Uuid const& parent
                           , std::vector< Uuid > const& children )
    -> Result< void >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
            auto const pchildren = fetch_children( parent );
            auto const cset = children
                            | to< UuidSet >();
            BC_ASSERT( cset == pchildren );
        })
    ;

    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_ENSURE( rv, exists( parent ), error_code::network::invalid_parent );
    KMAP_ENSURE( rv, fetch_children( parent ) == ( children | to< UuidSet >() ), error_code::network::invalid_parent );

    auto const order_ids = children
                         | views::transform( [ & ]( auto const& e ){ return resolve( e ); } )
                         | views::transform( to_ordering_id )
                         | to_vector;
    auto& db = database();

    db.update_child_ordering( parent
                            , order_ids );

    rv = outcome::success();

    return rv;
}

// Returns previously selected node.
// TODO: Add unit tests for:
//   - node coloring
auto Kmap::select_node( Uuid const& id )
    -> Result< Uuid > 
{
    KMAP_PROFILE_SCOPE();

    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( selected_node() == id );
            // assert viewport is centered...
            // assert nw is focused...
            // assert jump stack is increased...(possibly, depends)
        })
    ;

    KMAP_ENSURE( rv, exists( id ), error_code::network::invalid_node );

    auto const prev_selected = selected_node();
    auto& nw = network();

    {
        auto const visible_nodes = fetch_visible_nodes_from( id );
        auto const visible_node_set = UuidSet{ visible_nodes.begin()
                                             , visible_nodes.end() };
        auto create_edge = [ & ]( auto const& nid )
        {
            if( auto const pid = fetch_parent( nid )
              ; pid
             && visible_node_set.count( pid.value() ) != 0
             && !nw.edge_exists( pid.value()
                               , nid ) )
            {
                nw.add_edge( pid.value()
                           , nid );
            }
        };
        auto const to_title = [ & ]( auto const& e )
        {
            auto const title = fetch_title( resolve( e ) );
            BC_ASSERT( title );
            auto const child_count = fetch_children( e ).size();

            return fmt::format( "{} ({})"
                              , title.value()
                              , child_count );
        };

        // This is an efficient hack. It appears recent versions of visjs will render hierarchy based on node chronology
        // So, to get around this, all nodes are deleted for each movement. Then, recreated, in order.
        nw.remove_nodes();

        // Must be created in order, to work correctly with visjs's hierarchy mechanism.
        for( auto const& cid : visible_nodes )
        {
            BC_ASSERT( exists( cid ) );
            nw.create_node( cid, to_title( cid ) );
            create_edge( cid );
        }

        // Revert previous selected node to unselected color.
        if( visible_node_set.contains( prev_selected ) )
        {
            nw.color_node_background( prev_selected
                                    , Color::white );
        }

        for( auto const& e : visible_nodes )
        {
            color_node( e ); 

            KMAP_TRY( nw.change_node_font( e 
                                         , get_appropriate_node_font_face( e )
                                         , Color::black ) );
        }
    }

    nw.color_node_background( id
                            , Color::black );
    KMAP_TRY( nw.change_node_font( id
                                 , get_appropriate_node_font_face( id )
                                 , Color::white ) );
    nw.select_node( id );
    nw.center_viewport_node( id );
    nw.focus();
    auto id_abs_path = absolute_path_uuid( id );
    auto breadcrumb = id_abs_path
                    | views::drop_last( 1 )
                    | to< UuidVec >();
    KMAP_LOG_LINE();
    set_fwd_breadcrumb( to_heading_path_flat( *this, breadcrumb ) );
    KMAP_LOG_LINE();
    load_preview( id ); // Note: load preview must be after nw.select_node(), as it uses fetch_descendant, which uses selected_node, which must exist!
    KMAP_LOG_LINE();

    rv = prev_selected;

    return rv;
}

auto Kmap::swap_nodes( Uuid const& t1
                     , Uuid const& t2 )
    -> Result< UuidPair >
{
    auto rv = KMAP_MAKE_RESULT( UuidPair );

    BC_CONTRACT()
        BC_POST([ &
                , t1_pos = BC_OLDOF( fetch_ordering_position( t1 ) )
                , t2_pos = BC_OLDOF( fetch_ordering_position( t2 ) )
                , t1_parent = BC_OLDOF( fetch_parent( t1 ) )
                , t2_parent = BC_OLDOF( fetch_parent( t2 ) ) ]
        {
            if( rv )
            {
                BC_ASSERT( is_child( t1_parent->value(), t2 ) );
                BC_ASSERT( is_child( t2_parent->value(), t1 ) );
                BC_ASSERT( *t1_pos == fetch_ordering_position( t2 ) );
                BC_ASSERT( *t2_pos == fetch_ordering_position( t1 ) );
            }
        })
    ;

    // TODO: Can use Boost.Outcome Custom Payload to denote which node is not found.
    KMAP_ENSURE( rv, exists( t1 ), error_code::node::not_found );
    KMAP_ENSURE( rv, exists( t2 ), error_code::node::not_found );
    KMAP_ENSURE( rv, !is_root( t1 ), error_code::node::is_root );
    KMAP_ENSURE( rv, !is_root( t2 ), error_code::node::is_root );
    KMAP_ENSURE( rv, !is_lineal( t1, t2 ), error_code::node::is_lineal );
    KMAP_ENSURE( rv, !is_lineal( t2, t1 ), error_code::node::is_lineal );

    auto const t1_pos = fetch_ordering_position( t1 );
    auto const t2_pos = fetch_ordering_position( t2 );
    auto const t1_parent = fetch_parent( t1 ).value();
    auto const t2_parent = fetch_parent( t2 ).value();

    if( t1_parent != t2_parent ) // No movement is required if nodes are siblings; only repositioning.
    {
        KMAP_TRY( move_node( t1, t2_parent ) );
        KMAP_TRY( move_node( t2, t1_parent ) );
    }

    set_ordering_position( t1, *t2_pos ); 
    set_ordering_position( t2, *t1_pos ); 

    rv = std::pair{ t2, t1 };

    return rv;
}

auto Kmap::jump_to( Uuid const& id )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
        BC_POST([ & ]
        {
        })
    ;

    auto& js = jump_stack();
    auto const selected = selected_node();

    select_node( id );
    js.jump_in( selected );
}

auto Kmap::select_node( Heading const& heading )
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};

    if( auto const leaf = fetch_leaf( heading )
      ; leaf )
    {
        select_node( *leaf );
        rv = leaf;
    }

    return rv;
}

auto Kmap::selected_node() const
    -> Uuid
{
    return network().selected_node();
}

auto Kmap::load_preview( Uuid const& id )
    -> void 
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
            BC_ASSERT( database().fetch_body( resolve( id ) ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    KMAP_LOG_LINE();
    auto& db = database();
    KMAP_LOG_LINE();
    auto& tv = text_area();
    KMAP_LOG_LINE();
    auto const body = *db.fetch_body( resolve( id ) );

    KMAP_LOG_LINE();
    io::print( "loading_preview: {}\n", body );
    tv.show_preview( markdown_to_html( body ) );
    KMAP_LOG_LINE();
}

auto Kmap::on_leaving_editor()
    -> void
{
    auto& tv = text_area();
    auto const contents = tv.editor_contents();
    auto const rid = resolve( selected_node() );

    BC_ASSERT( exists( rid ) );

    database().update_body( rid, contents );
    canvas().hide( canvas().editor_pane() );
    select_node( selected_node() ); // Ensure the newly added preview is updated.
}

auto Kmap::node_fetcher() const
    -> NodeFetcher
{
    return NodeFetcher{ *this };
}

auto Kmap::node_view() const
    -> NodeView
{
    return node_view( root_node_id() );
}

auto Kmap::node_view( Uuid const& root ) const
    -> NodeView
{
    return NodeView{ *this, root };
}

auto Kmap::node_view( Uuid const& root
                    , Uuid const& selected ) const // TODO: Replace root, selected with "Lineage", to enforce constraint that root is ancestor of selected.
    -> NodeView
{
    return NodeView{ *this, make< Lineal >( *this, root, selected ).value() };
}

auto Kmap::focus_network()
    -> void
{
    auto& nw = network();

    select_node( nw.selected_node() );
}

auto Kmap::update_heading( Uuid const& id
                         , Heading const& heading )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    auto& db = database();
    auto const rid = resolve( id );

    db.update_heading( rid
                     , heading );
}

auto Kmap::update_title( Uuid const& id
                       , Title const& title )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: this function should return an outcome rather than fail with precondition.
        })
    ;

    auto& db = database();
    auto const rid = resolve( id );

    db.update_title( rid
                   , title );

    if( auto& nw = network()
      ; nw.exists( rid ) )
    {
        nw.update_title( rid
                       , title );
    }
}

auto Kmap::update_alias( Uuid const& from
                       , Uuid const& to )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( from ) );
            BC_ASSERT( exists( to ) );
        })
    ;

    // TODO: Deleting and remaking all does not seem optimal way to update.
    KMAP_TRY( delete_node( make_alias_id( from, to ) ) );

    rv = create_alias( from, to );

    return rv;
}

// Note: I would like to redesign the whole alias routine such that "updates" are never needed. Where aliases aren't "maintained",
// as stateful, but rather are treated as true references, but that is a great undertaking, so updates will serve in the meantime.
// TODO: Add test case to check when 'descendant' is an alias. I to fix this by resolving it. I suspect there are other places that suffer the same way.
auto Kmap::update_aliases( Uuid const& descendant )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( descendant ) );
        })
    ;

    auto const rdescendant = resolve( descendant );

    for( auto const& id : fetch_aliased_ancestry( rdescendant ) )
    {
        auto const& db = database();
        auto const dsts = db.fetch_alias_destinations( id );

        for( auto const& dst : dsts )
        {
            update_alias( id
                        , dst );
        }
    }
}

auto Kmap::count_ancestors( Uuid const& node ) const
   -> uint32_t
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( node ) );
            BC_ASSERT( node == root_node_id()
                    || fetch_parent( node ) );
        })
    ;

    auto rv = uint32_t{ 1 };
    auto parent = fetch_parent( node );

    while( parent )
    {
        ++rv;

        parent = fetch_parent( parent.value() );
    }

    return rv;
}

auto Kmap::count_descendants( Uuid const& root ) const
   -> uint32_t
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( root ) );
        })
    ;

    auto const& children = fetch_children( root );

    if( children.empty() )
    {
        return 0;
    }
    else
    {
        auto rv = uint32_t{};

        for( auto const& c : children )
        {
            rv += 1 + count_descendants( c );
        }

        return rv;
    }
}

// TODO: impl. body belongs in path.cpp
auto Kmap::distance( Uuid const& ancestor
                   , Uuid const& descendant ) const
    -> uint32_t
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( ancestor ) );
            BC_ASSERT( exists( descendant ) );
            BC_ASSERT( is_lineal( ancestor, descendant ) );
        })
    ;

    auto rv = uint32_t{};
    auto traveler = Optional< Uuid >{ descendant };

    while( traveler )
    {
        if( traveler != ancestor )
        {
            traveler = to_optional( fetch_parent( traveler.value() ) );

            ++rv;
        }
        else
        {
            traveler.reset();
        }
    }

    return rv;
}

auto Kmap::color_node( Uuid const& id
                     , Color const& color )
   -> void
{
    auto& nw = network();

    nw.color_node_border( id
                        , color );
}

auto Kmap::color_node( Uuid const& id )
   -> void
{
    auto const card = count_ancestors( id );
    BC_ASSERT( card > 0 );
    auto const color = color_level_map[ ( card - 1 ) % color_level_map.size() ];

    color_node( id
              , color );
}

auto Kmap::color_all_visible_nodes()
    -> void
{
    auto const& nw = network();

    for( auto const& n : nw.nodes() )
    {
        color_node( n );
    }
}

auto Kmap::is_child_internal( Uuid const& parent
                            , Uuid const& id ) const
    -> bool
{
    auto const cids = fetch_children( parent );

    return 0 != count( cids
                     , id );
}

auto Kmap::is_child( std::vector< Uuid > const& parents
                   , Uuid const& child ) const
    -> bool
{
    for( auto const& p : parents )
    {
        if( is_child( p
                    , child ) )
        {
            return true;
        }
    }

    return false;
}

auto Kmap::is_child_internal( Uuid const& parent
                            , Heading const& heading ) const
    -> bool
{
    return bool{ fetch_child( parent
                            , heading ) };
}

auto Kmap::is_alias( Uuid const& id ) const
    -> bool
{
    return aliases_.count( id ) != 0;
}

auto Kmap::is_alias( Uuid const& src
                   , Uuid const& dst ) const
    -> bool
{
    return database().alias_exists( src, dst );
}

// TODO: Better named "is_root_alias"? To be consistent?
/// Parent of alias is non-alias.
auto Kmap::is_top_alias( Uuid const& id ) const
    -> bool
{
    if( !is_alias( id ) )
    {
        return false;
    }

    auto const pid = fetch_parent( id );

    return pid && !is_alias( pid.value() );
}

auto Kmap::is_leaf_node( Uuid const& id ) const
    -> bool
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
    ;

    return fetch_children( id ).empty();
}

auto Kmap::is_lineal( Uuid const& ancestor
                    , Uuid const& descendant ) const
    -> bool
{
    return kmap::is_lineal( *this
                          , ancestor
                          , descendant );
}

auto Kmap::is_lineal( Uuid const& ancestor
                    , Heading const& descendant ) const
    -> bool
{
    return kmap::is_lineal( *this
                          , ancestor
                          , descendant );
}

auto Kmap::is_direct_descendant( Uuid const& ancestor
                               , Heading const& path ) const
    -> bool
{
    return kmap::is_direct_descendant( *this
                                     , ancestor
                                     , path );
}

auto Kmap::is_ancestor( Uuid const& root
                      , Uuid const& node ) const
    -> bool
{
    return kmap::is_ancestor( *this, root, node );
}

// TODO: This is semantically identical to is_lineal. Is there any good reason to keep it?
auto Kmap::is_in_tree( Uuid const& root
                     , Uuid const& node ) const
    -> bool
{
    return is_lineal( root
                    , node );
}

auto Kmap::is_root( Uuid const& node ) const
    -> bool
{
    return root_node_id() == node;
}

// Resolves alias down to its origin.
auto Kmap::resolve( Uuid const& id ) const
    -> Uuid
{
    auto rv = Uuid{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( !is_alias( rv ) );
        })
    ;

    if( !is_alias( id ) )
    {
        rv = id;
    }
    else
    {
        rv = id;

        // TODO: Is it possible for an alias to point to another alias? If the
        // create_alias() function always resolves its ids, it would seem not -
        // which would make this while check superfluous.
        while( is_alias( rv ) )
        {
            auto const pid = fetch_parent( rv );

            assert( pid );

            rv = alias_source( pid.value()
                             , rv );
        }
    }

    return rv;
}

auto Kmap::delete_children( Uuid const& parent )
    -> Result< void >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( fetch_children( parent ).empty() );
        })
    ;

    for( auto const& child : fetch_children( parent ) )
    {
        KMAP_TRY( delete_node( child ) );
    }

    return outcome::success();
}

// Returns a recommended node to select following the deletion of "id".
// TODO: Make test case for when node to be deleted is selected, when it's not, and when it's a descendant of the node to be deleted.
auto Kmap::delete_node( Uuid const& id )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !exists( id ) );
        })
    ;

    KMAP_ENSURE( rv, exists( id ), error_code::node::not_found );
    KMAP_ENSURE( rv, id != root_node_id(), error_code::node::is_root );
    KMAP_ENSURE( rv, !is_alias( id ) || is_top_alias( id ), error_code::node::is_nontoplevel_alias );

    auto const parent = fetch_parent( id ).value();
    auto const selected = selected_node();

    if( id == selected
     || is_ancestor( id, selected ) )
    {
        auto const next_sel = KMAP_TRY( fetch_next_as_if_deleted( id ) );

        KMAP_TRY( select_node( next_sel ) );

        rv = next_sel;
    }
    else
    {
        rv = selected; // If not deleting selected, just return selected.
    }

    delete_node_internal( id );

    if( !is_alias( id ) )
    {
        update_aliases( parent );
    }

    return rv;
}

auto Kmap::delete_node_internal( Uuid const& id )
    -> Result< void >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            // May be exclusively an alias (and not resolve to an underlying node b/c the node has already been deleted).
            BC_ASSERT( exists( id ) || is_alias( id ) ); // TODO: This should rather check and return an error.
            BC_ASSERT( id != root_node_id() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !exists( id ) );
        })
    ;

    auto rv = KMAP_MAKE_RESULT( void );

    {
        auto& db = database();

        // Delete children.
        for( auto const& e : fetch_children( id ) )
        {
            KMAP_TRY( delete_node_internal( e ) );
        }
        // Delete node.
        if( is_alias( id ) )
        {
            KMAP_TRY( delete_alias( id ) );
        }
        else
        {
            for( auto const& dst : database().fetch_alias_destinations( id ) )
            {
                KMAP_TRY( delete_node( make_alias_id( id, dst ) ) );
            }

            db.remove( id );
        }
    }

    rv = outcome::success();

    return rv;
}

auto Kmap::delete_alias( Uuid const& id )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( id != root_node_id() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( !is_alias( id ) );
        })
    ;

    KMAP_ENSURE_MSG( rv, is_alias( id ), error_code::node::invalid_alias, to_string( id ) );

    rv = KMAP_TRY( fetch_next_as_if_deleted( id ) );
 
    auto const dst = fetch_parent( id );
    BC_ASSERT( dst );

    if( is_top_alias( id ) ) // Must precede erasing alias from the cache, as it is dependent on it.
    {
        auto const src = resolve( id );
        auto& db = database();

        db.remove_alias( src
                       , dst.value() );
    }

    aliases().erase( id );
    alias_parents_.erase( id );
    alias_children_.erase( [ & ]
    {
        auto const er = alias_children_.equal_range( dst.value() );
        BC_ASSERT( er.first != er.second );
        auto r = er.first;

        for( auto it = er.first
           ; it != er.second
           ; ++it )
        {
            if( it->second == id )
            {
                r = it;
                break;
            }
        }

        return r;
    }() );

    return rv;
}

/// Call this after a change in the tree has occurred.
auto Kmap::update( Uuid const& id )
    -> void
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) ); // TODO: This should rather check and return an error.
        })
        BC_POST([ & ]
        {
        })
    ;
    
    // If an alias... update alias.
    // TODO! Implment this!
    assert( false ); // Use update_subtree? Trouble with updating a node and not it's children is that it may have moved or been deleted!
}

auto Kmap::jump_in()
    -> void//Optional< Uuid >
{
    auto& js = jump_stack();

    if( auto const dst = js.jump_in()
      ; dst )
    {
        select_node( *dst );
    }
}

auto Kmap::jump_out()
    -> Optional< Uuid >
{
    auto rv = Optional< Uuid >{};
    auto& js = jump_stack();

    if( auto const dst = js.jump_out()
      ; dst )
    {
        select_node( *dst );
        rv = dst;
    }

    return rv;
}

auto Kmap::jump_stack()
    -> JumpStack&
{
    return jump_stack_;
}

auto Kmap::jump_stack() const
    -> JumpStack const&
{
    return jump_stack_;
}

// TODO: Should this ensure that if the moved node is the selected node, it updates the selected as well? I believe this is only relevant for the alias case.
//       In essence, this can invalidate IDs for aliases.
auto Kmap::move_node( Uuid const& from
                    , Uuid const& to )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
        BC_POST([ &
                , from_alias = is_alias( from ) ]
        {
            if( rv )
            {
                if( !from_alias )
                {
                    BC_ASSERT( is_child( to
                                       , from ) );
                }
            }
        })
    ;

    KMAP_ENSURE( rv, exists( from ), error_code::network::invalid_node );
    KMAP_ENSURE( rv, exists( to ), error_code::network::invalid_node );
    KMAP_ENSURE( rv, !is_child( to, fetch_heading( from ).value() ), error_code::network::invalid_heading ); // TODO: Replace invalid_heading with duplicate_heading. See create_node::duplicate_child_heading.
    KMAP_ENSURE( rv, !is_alias( to ), error_code::network::invalid_node );
    KMAP_ENSURE( rv, from != root_node_id(), error_code::network::invalid_node );
    KMAP_ENSURE( rv, !is_ancestor( from, to ), error_code::network::invalid_node );

    auto const from_parent = fetch_parent( from ).value();
    auto const from_heading = fetch_heading( from ).value();

    if( is_child( to, from ) )
    {
        rv = from; // Already moved "self-move". Nothing to do.
    }
    else if( is_alias( from ) )
    {
        auto const rfrom = resolve( from );

        KMAP_TRY( delete_alias( from ) );

        auto const alias = create_alias( rfrom, to );

        BC_ASSERT( alias );

        rv = alias.value();
    }
    else
    {
        auto& db = database();

        // TODO: It's important to have a fail-safe for this routine. If remove
        // succeeds, but, for some reason, add fails, the remove should revert (or
        // vice-versa), otherwise we're at risk of having a dangling node.
        db.remove_child( from_parent 
                       , from );
        db.add_child( to
                    , from );

        rv = to;
    }

    if( rv )
    {
        // TODO: Rather than remove from the network here, place the burden of responsibility on Kmap::select_node
        //       This represents a corner-case for the present impl. of Kmap::select_node, in that the visible_nodes
        //       will rightly return "from", but since it hasn't left the network, but just moved, the network will not recreate it,
        //       meaning it will not be placed in the correct place.
        auto& nw = network();

        if( nw.exists( from ) )
        {
            nw.remove_edge( from_parent
                          , from );

            if( nw.exists( to ) )
            {
                nw.add_edge( to, from );
            }
        }
    }

    return rv;
}

auto Kmap::move_node( Uuid const& from
                    , Heading const& to )
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );
    auto const to_id = fetch_leaf( root_node_id() 
                                 , selected_node()
                                 , to );

    if( to_id )
    {
        rv = move_node( from
                      , *to_id );
    }

    return rv;
}

auto Kmap::fetch_aliased_ancestry( Uuid const& id ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( rv.size() <= count_ancestors( id ) );
        })
    ;

    auto child = id;
    auto parent = fetch_parent( child );
    
    while( parent )
    {
        if( auto const aliases_from = fetch_aliases_to( child )
          ; aliases_from.size() > 0 )
        {
            rv.emplace_back( child );
        }

        child = parent.value();
        parent = fetch_parent( child );
    }

    return rv;
}

auto Kmap::are_siblings( Uuid const& n1
                       , Uuid const& n2 ) const
    -> bool
{
    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( exists( n1 ) );
            BC_ASSERT( exists( n2 ) );
        })
    ;

    if( auto const p1 = fetch_parent( n1 )
      ; p1 )
    {
        if( auto const p2 = fetch_parent( n2 )
          ; p2 )
        {
            if( p1.value() == p2.value() )
            {
                return true;
            }
        }
    }

    return false;
}

auto Kmap::has_descendant( Uuid const& ancestor 
                         , std::function< bool( Uuid const& ) > const& pred ) const
    -> bool
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( ancestor ) );
        })
    ;

    auto rv = false;

    if( pred( ancestor ) )
    {
        rv = true;
    }
    else
    {
        for( auto const& cid : fetch_children( ancestor ) )
        {
            rv = has_descendant( cid, pred );

            if( rv )
            {
                break;
            }
        }
    }

    return rv;
}

// TODO: Should this rather be named the `id`-inclusive `is_lineage_aliased`?
auto Kmap::is_ancestry_aliased( Uuid const& id ) const
    -> bool
{
    // Note: more efficient to bail on first alias encountered.
    return !fetch_aliased_ancestry( id ).empty();
}

auto Kmap::fetch_parent( Uuid const& child ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT_EC( Uuid, error_code::network::invalid_parent );
    auto& db = database();

    if( auto const pid = db.fetch_parent( child )
      ; pid )
    {
        rv = pid.value();
    }
    else if( is_alias( child ) )
    {
        rv = alias_parents_.find( child )->second;
    }

    return rv;
}

auto Kmap::fetch_parent( Uuid const& root
                       , Uuid const& child ) const // TODO: Replace with Descendant? Such would make this a no-fail routine. Perhaps kmap.[get_]parent( Descendant )?
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT_EC( Uuid, error_code::network::invalid_parent );

    KMAP_ENSURE( rv, is_lineal( root, child ), error_code::network::invalid_lineage );

    if( child != root )
    {
        rv = fetch_parent( child );
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::network::invalid_parent, child );
    }

    return rv;
}

auto Kmap::fetch_title( Uuid const& id ) const
    -> Result< Title >
{
    auto rv = KMAP_MAKE_RESULT( Title );

    if( exists( id ) )
    {
        rv = database().fetch_title( resolve( id ) ).value();
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::network::invalid_node, id );
    }

    return rv;
}

auto Kmap::fetch_nodes() const
    -> UuidUnSet
{
    auto const& as = aliases();
    auto nodes = database().fetch_nodes();

    nodes.insert( as.begin()
                , as.end() );
                
    return nodes;
}

// TODO: This needs some thought about whether to check the heading of 'descendant' with the heading path i.e., if 'descendant' is 'child', and ancestors are 'parent' and 'grandparent', should the heading be 'child.parent.grandparent'? 
// TODO: I don't think this algorithm safely handles alias parents. Simply assumed that all parents will have valid headings.
auto Kmap::fetch_ancestor( Uuid const& descendant
                         , InvertedHeading const& heading ) const
    -> Optional< Uuid >
{
    auto const& db = database();
    auto const split = heading
                     | views::split( '.' )
                     | views::transform( []( auto const& e ){ return to< std::string >( e ); } )
                     | to< HeadingPath >();
    auto rv = to_optional( fetch_parent( descendant ) );

    if( !rv )
    {
        return nullopt;
    }

    if( split.size() == 1 )
    {
        if( db.fetch_heading( *rv ) == heading )
        {
            return rv;
        }
        else
        {
            return nullopt;
        }
    }
    else
    {
        for( auto const& pheading : split )
        {
            if( pheading != *db.fetch_heading( rv.value() ) )
            {
                rv = nullopt;
                break;
            }
            else if( rv = to_optional( fetch_parent( rv.value() ) )
                   ; !rv )
            {
                break;
            }
        }

        return rv;
    }
}

auto Kmap::fetch_body( Uuid const& id ) const
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    if( exists( id ) )
    {
        return *database().fetch_body( resolve( id ) );
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::network::invalid_node, id );
    }

    return rv;
}

auto Kmap::fetch_child( Uuid const& parent 
                      , Heading const& heading ) const
   -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
        })
    ;

    if( exists( parent ) )
    {
        for( auto const& e : fetch_children( parent ) )
        {
            if( heading == fetch_heading( e ).value() )
            {
                rv = e;

                break;
            }
        }
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::node::parent_not_found, parent );
    }

    return rv;
}

auto Kmap::fetch_alias_children( Uuid const& parent ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            if( alias_children_.size() != alias_parents_.size()  )
            {
                fmt::print( "alias_chilren:{} != alias_parents:{}\n", alias_children_.size(), alias_parents_.size() );
                for( auto ac : alias_children_ )
                {
                    fmt::print( "ac: {}:{}\n", to_string( ac.first ), fetch_heading( ac.first ).value() );
                }
            }
            BC_ASSERT( alias_children_.size() == alias_parents_.size() );

            for( auto const& e : rv )
            {
                BC_ASSERT( is_alias( e ) );
            }
        })
    ;

    auto const er = alias_children_.equal_range( parent );

    for( auto it = er.first
       ; it != er.second
       ; ++it )
    {
        rv.emplace_back( it->second );
    }

    return rv;
}

auto Kmap::fetch_children( Uuid const& parent ) const
    -> kmap::UuidSet
{ 
    auto rv = kmap::UuidSet{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( alias_children_.size() == alias_parents_.size() );

            for( auto const& e : rv )
            {
                BC_ASSERT( exists( e ) );
            }
        })
    ;

    auto const db_children = [ & ]
    {
        if( is_alias( parent ) )
        {
            return kmap::UuidSet{};
        }
        else
        {
            auto const& db = database();

            return db.fetch_children( parent );
        }
    }();
    auto const alias_children = fetch_alias_children( parent );

    return views::concat( db_children
                       , alias_children )
         | to< kmap::UuidSet >();
}

auto Kmap::fetch_children( Uuid const& root
                         , Heading const& parent ) const
    -> kmap::UuidSet
{ 
    auto rv = kmap::UuidSet{};

    if( auto const pn = fetch_descendant( root, parent )
      ; pn )
    {
        rv = fetch_children( pn.value() );
    }

    return rv;
}

auto Kmap::fetch_parent_children( Uuid const& id ) const
    -> kmap::UuidSet
{ 
    return kmap::fetch_parent_children( *this
                                      , id );
}

auto Kmap::fetch_parent_children_ordered( Uuid const& id ) const
    -> kmap::UuidVec
{ 
    return kmap::fetch_parent_children_ordered( *this
                                              , id );
}

auto Kmap::fetch_siblings( Uuid const& id ) const
    -> kmap::UuidSet
{ 
    return kmap::fetch_siblings( *this
                               , id );
}

// TODO: this is to be eventually superceded by: node | siblings | drop( node ) | ordered
auto Kmap::fetch_siblings_ordered( Uuid const& id ) const
    -> kmap::UuidVec
{ 
    return kmap::fetch_siblings_ordered( *this
                                       , id );
}

auto Kmap::fetch_siblings_inclusive( Uuid const& id ) const
    -> kmap::UuidSet
{ 
    return kmap::fetch_siblings_inclusive( *this
                                         , id );
}

// TODO: this is to be eventually superceded by: node | siblings | ordered
auto Kmap::fetch_siblings_inclusive_ordered( Uuid const& id ) const
    -> kmap::UuidVec
{ 
    return kmap::fetch_siblings_inclusive_ordered( *this
                                                 , id );
}

auto Kmap::fetch_children_ordered( Uuid const& root
                                 , Heading const& path ) const
    -> std::vector< Uuid >
{
    auto rv = UuidVec{};

    if( auto const pn = fetch_descendant( root, path )
      ; pn )
    {
        rv = fetch_children_ordered( pn.value() );
    }

    return rv;
}

auto Kmap::fetch_children_ordered( Uuid const& parent ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( parent ) ); // TODO: Warranted, or should I just return {}?
            BC_ASSERT( database().fetch_child_ordering( resolve( parent ) ).size() == fetch_children( parent ).size() );
        })
        BC_POST([ & ]
        {
            BC_ASSERT( rv.size() == fetch_children( parent ).size() );
        })
    ;

    auto const& db = database();
    auto const children = fetch_children( parent );
    auto const mapped = children
                        | views::transform( [ & ]( auto const& child )
                        {
                            auto const rchild = resolve( child );
                            auto const order_id = to_ordering_id( rchild );

                            return std::make_pair( order_id
                                                 , child );
                        } )
                        | to_vector;
    auto const ordered = db.fetch_child_ordering( resolve( parent ) );

    rv = ordered
       | views::transform( [ & ]( auto const& e )
       {
           auto const it = find( mapped
                               , e
                               , &std::pair< std::string, Uuid >::first );

           assert( it != mapped.end() );

           return it->second;
       } )
       | to< std::vector< Uuid > >();

    return rv;
}

auto Kmap::fetch_heading( Uuid const& id ) const
    -> Result< Heading >
{
    auto rv = KMAP_MAKE_RESULT( Heading );

    if( exists( id ) )
    {
        rv = database().fetch_heading( resolve( id ) ).value();
    }
    else
    {
        rv = KMAP_MAKE_ERROR_UUID( error_code::network::invalid_node, id );
    }

    return rv;
}

auto Kmap::fetch_nodes( Heading const& heading ) const
    -> UuidSet
{
    return database().fetch_nodes( heading );
}

auto Kmap::fetch_nearest_ancestor( Uuid const& root
                                 , Uuid const& leaf
                                 , std::regex const& geometry ) const
    -> Result< Uuid >
{
    return kmap::fetch_nearest_ancestor( *this
                                       , root
                                       , leaf
                                       , geometry );
}

auto Kmap::fetch_next_as_if_deleted( Uuid const& node ) const
    -> Result< Uuid >
{
    auto rv = KMAP_MAKE_RESULT( Uuid );

    if( auto const above = fetch_above( node )
      ; above )
    {
        rv = above.value();
    }
    else if( auto const below = fetch_below( node )
           ; below )
    {
        rv = below.value();
    }
    else if( auto const parent = fetch_parent( node )
           ; parent )
    {
        rv = parent.value();
    }
    
    return rv;
}

auto Kmap::fetch_visible_nodes_from( Uuid const& id ) const
    -> std::vector< Uuid >
{ 
    KMAP_PROFILE_SCOPE();

    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            for( auto const& e : rv )
            {
                BC_ASSERT( exists( e ) );
            }
            auto const s = UuidSet{ rv.begin()
                                  , rv.end() };
            BC_ASSERT( rv.size() == s.size() );
        })
    ;

    auto const lineage = fetch_ancestral_lineage( id, 5 ); // TODO: This max should be drawn from settings.
    auto const lineage_set = lineage | to< std::set< Uuid > >();

    rv.emplace_back( lineage[ 0 ] );

    for( auto const& e : lineage )
    {
        if( auto const all_ordered = fetch_children_ordered( e )
          ; all_ordered.size() > 0 )
        {
            auto const all_ordered_set = all_ordered | to< std::set< Uuid > >();
            auto const sub_ordered = [ & ]() -> std::vector< Uuid >
            {
                auto const range_size = 10u;

                if( auto const intersect = views::set_intersection( all_ordered_set
                                                                  , lineage_set )
                  ; begin( intersect ) != end( intersect ) )
                {
                    // TODO: BC_ASSERT( distance( intersect ) == 1 );

                    auto const lineal_child = *begin( intersect );

                    return select_median_range( all_ordered
                                              , lineal_child
                                              , range_size );
                }
                else
                {
                    return select_median_range( all_ordered
                                              , range_size );
                }

                return {};
            }();

            rv.insert( rv.end()
                     , sub_ordered.begin()
                     , sub_ordered.end() );
        }
    }

    // TODO:
    // Order by distance from root, or is this already guaranteed? Or should it be the responsiblity of the caller?
    // If it's already guaranteed, it should be a postcondition.
    // Sort: left-to-right, up-to-down.
    // ranges::sort( rv
    //             , [ this, root = lineage.front() ]( auto const& lhs, auto const& rhs )
    //             {
    //                 auto const lhs_dist = distance( root, lhs );
    //                 auto const rhs_dist = distance( root, rhs );
    //                 if( lhs_dist == rhs_dist )
    //                 {
    //                     auto const siblings = fetch_siblings_inclusive_ordered( lhs );

    //                     return find( siblings, lhs ) < find( siblings, rhs );
    //                 }
    //                 else
    //                 {
    //                     return lhs_dist < rhs_dist;
    //                 }
    //             } );

    return rv;
}

auto Kmap::descendant_leaves( Uuid const& root ) const
    -> std::vector< Uuid >
{
    auto rv = std::vector< Uuid >{};

    BC_CONTRACT()
        BC_POST([ & ]
        {
            BC_ASSERT( rv.size() == std::set< Uuid >( rv.begin()
                                                    , rv.end() )
                                                    . size() );
        })
    ;

    auto const children = fetch_children( root );

    if( children.empty() )
    {
        rv.emplace_back( root );
    }
    else
    {
        for( auto const& c : children )
        {
            auto const dls = descendant_leaves( c );

            rv.insert( rv.begin()
                     , dls.begin()
                     , dls.end() );
        }
    }

    return rv;
}

auto Kmap::descendant_paths( Uuid const& root ) const
    -> std::vector< HeadingPath >
{
    auto rv = std::vector< HeadingPath >{};
    auto const leaves = descendant_leaves( root );
    auto const paths = leaves
                     | views::transform( [ & ]( auto const& e ){ return absolute_path( e ); } )
                     | to< std::vector< HeadingPath > >();
    
    rv = paths;

    return rv;
}

auto Kmap::descendant_ipaths( Uuid const& root ) const
    -> std::vector< HeadingPath >
{
    auto const rv = descendant_paths( root );

    return rv
         | views::transform( [ & ]( auto const& e ){ return e | views::reverse | to< HeadingPath >(); } )
         | to< std::vector< HeadingPath > >();
}

auto Kmap::fetch_leaf( Uuid const& root
                     , Uuid const& selected
                     , Heading const& heading ) const
    -> Optional< Uuid >
{
    auto const ids = fetch_descendants( root
                                      , selected
                                      , heading );

    if( !ids )
    {
        return nullopt;
    }

    return { ids.value().back() }; // TODO: Clearly wrong. Simply grabbing the "last" of an ambiguous set is not correct.
}

auto Kmap::fetch_leaf( Heading const& heading ) const
    -> Optional< Uuid >
{
    return fetch_leaf( root_node_id()
                     , selected_node()
                     , heading );
}

// TODO: Replace ( root, selected ) with constrained Lineal type.
auto Kmap::fetch_descendant( Uuid const& root 
                           , Uuid const& selected
                           , Heading const& heading ) const
    -> Result< Uuid >
{
    return kmap::fetch_descendant( *this
                                 , root
                                 , selected
                                 , heading );
}

auto Kmap::fetch_descendant( Heading const& heading ) const
    -> Result< Uuid >
{
    return fetch_descendant( root_node_id()
                           , adjust_selected( root_node_id() )
                           , heading );
}

auto Kmap::fetch_descendants( Uuid const& root 
                            , Uuid const& selected
                            , Heading const& heading ) const
    -> Result< UuidVec >
{
    return kmap::fetch_descendants( *this
                                  , root
                                  , selected
                                  , heading );

}

auto Kmap::fetch_descendants( Heading const& heading ) const
    -> Result< UuidVec >
{
    return fetch_descendants( root_node_id()
                            , adjust_selected( root_node_id() )
                            , heading );
}

auto Kmap::fetch_descendant( Uuid const& root
                           , Heading const& heading ) const
    -> Result< Uuid >
{
    return kmap::fetch_descendant( *this
                                 , root
                                 , adjust_selected( root )
                                 , heading );
}

// TODO: The argument order should be root, heading.
// TODO: I reckon there's a deal of redundancy here between fetch_leaf and this.
auto Kmap::fetch_or_create_leaf( Uuid const& root
                               , Heading const& heading )
    -> Optional< Uuid >
{
    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( is_absolute( heading ) ); // This routine only excepts absolute paths.
        })
    ;

    auto pid = root;
    auto const headings = heading
                        | views::split( '.' )
                        | views::drop( 1 ) // Drop leading '.'
                        | views::transform( []( auto const& e ){ return to< std::string >( e ); } )
                        | to< StringVec >();

    if( ranges::distance( headings ) == 0 )
    {
        return nullopt;
    }

    {
        auto const sroot = fetch_heading( root );

        if( !sroot
         || sroot.value() != headings[ 0 ]  )
        {
            return nullopt;
        }
    }

    for( auto const& heading : headings 
                             | views::drop( 1 ) ) // Drop `<root>`
    {
        auto cid = fetch_child( pid
                              , heading );

        if( !cid )
        {
            cid = create_child( pid
                              , heading ).value();
        }

        pid = cid.value();
    }

    return { pid };
}

auto Kmap::fetch_or_create_descendant( Uuid const& root
                                     , Uuid const& selected
                                     , Heading const& heading ) 
    -> Result< Uuid >
{
    return kmap::fetch_or_create_descendant( *this
                                           , root
                                           , selected
                                           , heading );
}

auto Kmap::fetch_or_create_descendant( Uuid const& root
                                     , Heading const& heading ) 
    -> Result< Uuid >
{
    return kmap::fetch_or_create_descendant( *this
                                           , root
                                           , adjust_selected( root )
                                           , heading );
}

auto Kmap::fetch_or_create_leaf( Heading const& heading )
    -> Optional< Uuid >
{
    return fetch_or_create_leaf( root_node_id()
                               , heading );
}

auto Kmap::fetch_or_create_descendant( Heading const& heading ) 
    -> Result< Uuid >
{
    return fetch_or_create_descendant( root_node_id()
                                     , heading );
}

auto Kmap::fetch_genesis_time( Uuid const& id ) const
    -> Optional< uint64_t >
{
    return database().fetch_genesis_time( id );
}

auto Kmap::fetch_ordering_position( Uuid const& node ) const
    -> Optional< uint32_t >
{
    auto rv = Optional< uint32_t >{};
    auto const ordering = fetch_parent_children_ordered( node );
    
    if( auto const it = find( ordering
                            , node )
      ; it != end( ordering) )
    {
        rv = std::distance( ordering.begin(), it );
    }

    return rv;
}

auto Kmap::get_appropriate_node_font_face( Uuid const& id ) const
    -> std::string
{
    auto rv = std::string{};

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( exists( id ) );
        })
    ;

    if( is_top_alias( id ) )
    {
        rv = "ariel";
    }
    else
    {
        rv = "verdana";
    }

    return rv;
}

Kmap& Singleton::instance()
{
    if( !inst_ )
    {
        KMAP_LOG_LINE();
        try
        {
            inst_ = std::make_unique< Kmap >();
            if( !inst_ )
            {
                io::print( "failed to intitialize Kmap instance\n" );
            KMAP_LOG_LINE();
            }
        }
        catch( std::exception& e )
        {
            io::print( "Singleton::instance exception: {}\n", e.what() );
        }
        KMAP_LOG_LINE();
    }

    return *inst_;
}

std::unique_ptr< Kmap > Singleton::inst_ = {};

} // namespace kmap

