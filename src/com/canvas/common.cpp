/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/canvas/canvas.hpp>

#include <com/database/root_node.hpp>
#include <com/event/event.hpp>
#include <com/filesystem/filesystem.hpp>
#include <com/network/network.hpp>
#include <com/option/option.hpp>
#include <component.hpp>
#include <contract.hpp>
#include <error/master.hpp>
#include <error/network.hpp>
#include <error/node_manip.hpp>
#include <filesystem.hpp>
#include <path.hpp>
#include <path/act/abs_path.hpp>
#include <path/act/order.hpp>
#include <path/act/update_body.hpp>
#include <path/node_view2.hpp>
#include <test/util.hpp>
#include <util/json.hpp>
#include <util/macro.hpp>
#include <util/result.hpp>
#include <util/window.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <boost/filesystem.hpp>
#include <boost/json/parse.hpp>
#include <catch2/catch_test_macros.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/remove.hpp>
#include <range/v3/view/replace.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/stride.hpp>
#include <range/v3/view/transform.hpp>

#include <fstream>
#include <string>
#include <regex>

namespace bjn = boost::json;
namespace rvs = ranges::views;

// TODO: drop using namespace in favor of rvs.
using namespace ranges;


namespace kmap::com {

auto operator<<( std::ostream& lhs 
               , Dimensions const& rhs )
    -> std::ostream&
{
    lhs << io::format( "top:{}, bottom:{}, left:{}, right:{}", rhs.top, rhs.bottom, rhs.left, rhs.right );

    return lhs;
}

auto to_string( Orientation const& orientation )
    -> std::string
{
    switch( orientation )
    {
        default: KMAP_THROW_EXCEPTION_MSG( "invalid enum val" );
        case Orientation::horizontal: return "horizontal";
        case Orientation::vertical: return "vertical";
    }
}

auto fetch_pane( bjn::object const& layout )
    -> Result< Pane >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< Pane >();

    auto const heading = KTRY( fetch_string( layout, "heading" ) ); KM_RESULT_PUSH( "heading", heading );
    auto const id_str = KTRY( fetch_string( layout, "id" ) );
    auto const id = KTRY( uuid_from_string( id_str ) );
    auto const orientation_str = KTRY( fetch_string( layout, "orientation" ) );
    auto const orientation = KTRY( from_string< Orientation >( orientation_str ) );
    auto const base = KTRY( fetch_float( layout, "base" ) );
    auto const hidden = KTRY( fetch_bool( layout, "hidden" ) );
    auto const type = KTRY( fetch_string( layout, "type" ) );

    rv = Pane{ .id = id
             , .heading = heading
             , .division = Division{ .orientation = orientation
                                   , .base = static_cast< float >( base ) // TODO: Technically should ensure conversion acceptable, better yet constrianed between 0.0 and 1.0.
                                   , .hidden = hidden
                                   , .elem_type = type } };

    return rv;
}

auto fetch_pane( bjn::object const& layout
               , Uuid const& id )
    -> Result< Pane >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< Pane >();

    if( auto const id_str = KTRY( fetch_string( layout, "id" ) )
      ; id_str == to_string( id ) )
    {
        rv = KTRY( fetch_pane( layout ) );
    }
    else
    {
        for( auto const lsubdivs = fetch_subdivisions( layout )
           ; auto const& lsubdiv : lsubdivs )
        {
            if( auto const rp = fetch_pane( lsubdiv, id )
              ; rp )
            {
                rv = rp.value();

                break;
            }
        }
    }

    return rv;
}

auto fetch_parent_pane( Kmap const& km
                      , bjn::object const& layout
                      , Uuid const& root_pane
                      , Uuid const& child_pane )
    -> Result< Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< Uuid >();

    if( has_subdivision( layout, child_pane ) )
    {
        rv = root_pane;
    }
    else
    {
        auto const lsubdivs = fetch_subdivisions( layout );
        auto const subdiv_nodes = anchor::node( root_pane )
                                | view2::child( "subdivision" )
                                | view2::child
                                | act2::to_node_set( km );

        for( auto const& lsubdiv : lsubdivs )
        {
            if( auto const id_str = fetch_string( lsubdiv, "id" ) 
              ; id_str )
            {
                if( auto const id = uuid_from_string( id_str.value() ) 
                  ; id )
                {
                    if( subdiv_nodes.contains( id.value() ) )
                    {
                        if( auto const pp = fetch_parent_pane( km, lsubdiv, id.value(), child_pane )
                          ; pp )
                        {
                            rv = pp.value();

                            break;
                        }
                    }
                }
            }
        }
    }

    return rv;
}

auto fetch_subdivisions( bjn::object const& layout )
    -> std::vector< bjn::object >
{
    auto rv = std::vector< bjn::object >{};

    if( auto const arr = fetch_array( layout, "subdivision" )
      ; arr )
    {
        auto const& arrv = arr.value();

        rv = arrv
           | rvs::filter( []( auto const& e ){ return e.is_object(); } )
           | rvs::transform( []( auto const& e ){ return e.as_object(); } )
           | ranges::to< decltype( rv ) >();
    }

    return rv;
}

auto has_pane_id( bjn::object const& layout
                , Uuid const& id )
    -> bool
{
    return KTRYB( fetch_string( layout, "id" ) ) == to_string( id );
}

auto has_subdivision( bjn::object const& layout
                    , Uuid const& id )
    -> bool
{
    auto const subdivs = KTRYB( fetch_array( layout, "subdivision" ) );
    auto const pred = [ & ]( auto const& e )
    {
        KENSURE_B( e.is_object() );

        return has_pane_id( e.as_object(), id );
    };

    return ranges::find_if( subdivs.begin(), subdivs.end(), pred ) != subdivs.end();
}

auto load_initial_layout( FsPath const& path )
    -> Result< std::string >
{
    namespace fs = boost::filesystem;

    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "path", path.string() );

    auto rv = result::make_result< std::string >();

    rv = kmap::to_string( KTRY( open_ifstream( path, std::ios::binary ) ) );

    return rv;
}

} // namespace kmap::com

namespace kmap
{
    template<>
    auto from_string( std::string const& s )
        -> Result< com::Orientation >
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT_EC( com::Orientation, error_code::common::conversion_failed );

        if( s == "horizontal" )
        {
            rv = com::Orientation::horizontal;
        }
        else if( s == "vertical" )
        {
            rv = com::Orientation::vertical;
        }

        return rv;
    }
}