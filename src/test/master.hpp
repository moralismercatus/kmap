/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TEST_MASTER_HPP
#define KMAP_TEST_MASTER_HPP

#include <kmap.hpp>
#include <com/cli/cli.hpp>
#include <com/network/network.hpp>
#include <path/act/abs_path.hpp>
#include <util/concepts.hpp>
#include <contract.hpp>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

namespace kmap {

class Kmap;

auto run_pre_env_unit_tests()
    -> int;
auto run_unit_tests( std::vector< std::string > const& args )
    -> int;

namespace test {

// Use this fixture when completely resetting Kmap is required. For example, when Kmap fundamentals are being tested, or to ensure Kmap is fully reset.
struct ResetInstanceFixture
{
    ResetInstanceFixture();
    ~ResetInstanceFixture();
};

// Use this fixture when Kmap fundamentals are considered reliable.
// As each piece of data is tied to a node, it should be safe to assume that deleting all nodes (except root) is equivalent to
// reseting the map without having to decomission the DB. Speeds up testing, while also testing the delete function.
struct ClearMapFixture
{
    ClearMapFixture();
    ~ClearMapFixture();
};

template< typename... T >
[[ maybe_unused ]]
auto create_lineages( T&&... args )
    -> std::map< Heading, Uuid >
{
    KM_RESULT_PROLOG();

    auto rv = std::map< Heading, Uuid >{};
    auto const paths = std::vector< std::string >{ std::forward< T >( args )... };
    auto& kmap = Singleton::instance();
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );

    for( auto const& path : paths )
    {
        auto const desc = create_descendants( kmap, kmap.root_node_id(), nw->selected_node(), path ); BC_ASSERT( desc && !desc.value().empty() );
        auto const descv = desc.value();
        for( auto const& node : descv )
        {
            rv.emplace( KTRYE( anchor::abs_root
                             | view2::desc( node )
                             | act2::abs_path_flat( kmap ) )
                      , node );
        }
    }

    return rv;
}

auto select_each_descendant_test( Kmap& kmap
                                , Uuid const& node )
    -> bool;

} // namespace test

} // namespace kmap

#endif // KMAP_TEST_MASTER_HPP
